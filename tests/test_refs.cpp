/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2016 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

///
/// \file     test_refs.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/general_reference_wrapper.h"

using namespace dpr;
using namespace std;

template<typename T>
char* send(const T& data) {
	void* p;
	int count;

	static char buffer[65536];

	boost::iostreams::basic_array_sink<char> output(buffer, static_cast<std::size_t>(65536));
	boost::iostreams::stream<boost::iostreams::basic_array_sink<char>> output_stream(output);
	boost::archive::binary_oarchive oa(output_stream, boost::archive::no_header);

	oa << data.get();
	return buffer;
}

template<typename T>
void recv(char* buffer, T& data) {
	boost::iostreams::basic_array_source<char> source(buffer, static_cast<std::size_t>(65536));
	boost::iostreams::stream<boost::iostreams::basic_array_source <char>> s(source);
	boost::archive::binary_iarchive ia(s, boost::archive::no_header);

	ia >> data.get();
}

int n = 10;
int LIMIT = 5;

struct node {
	int i;
	node* left, *right;

	node() {
		i = 0;
		left = NULL;
		right = NULL;
	};

	~node() {
		delete left;
		delete right;
	}

	template<class Archive>
	void serialize(Archive& ar, const unsigned int file_version) {
		ar& i& left& right;
	}
};

struct Info : public DInfo<node*, 2> {
	Info() : DInfo<node *, 2>() { }

	bool is_base(const node* n) const {
		return n->left == NULL;
	}

	node*& child(int i, node* n) const {
		return (i == 0) ? n->left : n->right;
	}
};

struct Body : public EmptyBody<node*, unsigned long long int> {
	void pre(node* n) {
		n->i = 20;
	}

	unsigned long long int base(node* n) {
		n->i = 50;
		return n->i;
	}

	unsigned long long int post(node* n, unsigned long long int* r) {
		return r[0] + r[1] + n->i;
	}
};

node* build(int level) {
	if (level == 0)
		return NULL;
	node* n = new node();
	n->i = 5;
	n->left = build(level - 1);
	n->right = build(level - 1);
	return n;
}

void verify(node* root, int value1, int value2, int level = 0) {
	if ((root->i != value1 && root->left != NULL) || (root->i != value2 && root->left == NULL)) {
		printf("wrong value at level %d: %d != %d or %d\n", level, root->i, value1, value2);
		throw new exception();
	}
	if (root->left) {
		verify(root->left, value1, value2, level + 1);
		verify(root->right, value1, value2, level + 1);
	}
}

int height(node* root, int level = 0) {
	if (root->left == NULL)
		return level;
	else
		return height(root->left, level + 1);
}

void print_tree(node* root) {
	printf("%p(%d)=> left->[%p] %p, right->[%p] %p\n", root, root->i, &root->left, root->left, &root->right, root->right);
	if (root->left) {
		print_tree(root->left);
		print_tree(root->right);
	}
}

void change_tree(node* root) {
	root->i = 10;
	if (root->left) {
		change_tree(root->left);
		change_tree(root->right);
	}
}

node*& get_left(node* root) {
	return root->left;
}

int main(int argc, char** argv) {
	int provided, rank, ret = 0;
	int nthreads = 8;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (getenv("OMP_NUM_THREADS"))
		nthreads = atoi(getenv("OMP_NUM_THREADS"));
	tbb::task_scheduler_init init(nthreads);

	/*
	    node* root = build(n);
	    printf("before\n");
	    print_tree(root);
	    auto tal = general_reference_wrapper<node*>(get_left(root));
	    char* buffer = send(tal);
	    change_tree(root);
	    delete root->left;
	    recv(buffer, tal);
	    printf("after\n");
	    print_tree(root);
	*/
  
	node* root = build(n);
	unsigned long long int sum = dparallel_recursion<unsigned long long int>(root, Info(), Body(), partitioner::automatic(), GatherInput);
	printf("ended %d\n", rank);
	if (rank == 0) {
		int nprocs;
		MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
		try {
			verify(root, 20, 50);
			printf("sum = %lld\n", sum);
			printf("height = %d\n", height(root));
			printf("*SUCCESS*\n");
		} catch (exception) {
			printf(" FAILURE!\n");
                        ret = -1;
		}
		printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
	}

	MPI_Finalize();

	return ret;
}
