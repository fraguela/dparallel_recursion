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
/// \file     tsp.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <iostream>
#include <boost/serialization/split_member.hpp>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/Range.h"

#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION (scalable_malloc(sizeof(tree)))
#else
#define NODEALLOCATION
#endif

/* For copyright information, see olden_v1.0/COPYRIGHT */

extern int flag, nthreads, tasks_per_thread;

double median(double min, double max, int n, double fseed);
double uniform(double min, double max);

struct tree {

  union {
    struct {
	int sz;
	double x, y;

	struct tree *left, *right;
	struct tree *next, *prev;
    };
    struct {
      int num_, dir_;
      double min_x_, max_x_, min_y_, max_y_;
    };
  };
	tree(int num, int dir, double min_x, double max_x, double min_y, double max_y) :
		num_(num), dir_(dir), min_x_(min_x), max_x_(max_x), min_y_(min_y), max_y_(max_y) { }

	tree() /*: sz(0), left(NULL), right(NULL), next(NULL), prev(NULL) */ { }
  
  void pre() {
    double med;
    
    int num = num_;
    int dir = dir_;
    double min_x = min_x_;
    double max_x = max_x_;
    double min_y = min_y_;
    double max_y = max_y_;
    
    sz = num;
    prev = NULL;
    next = NULL;
    left = NULL;
    right = NULL;

    if(dir) {
      med = median(min_x, max_x, sz, min_y * max_y);
      x = med;
      y = uniform(min_y, max_y);
      if(sz > 1) {
        left = new NODEALLOCATION tree(sz / 2, !dir, min_x, med, min_y, max_y);
        right = new NODEALLOCATION tree(sz / 2, !dir, med, max_x, min_y, max_y);
      }
    } else {
      med = median(min_y, max_y, sz, min_x * max_x);
      y = med;
      x = uniform(min_x, max_x);
      if(sz > 1) {
        left = new NODEALLOCATION tree(sz / 2, !dir, min_x, max_x, min_y, med);
        right = new NODEALLOCATION tree(sz / 2, !dir, min_x, max_x, med, max_y);
      }
    }
    //printf("%d %d %f %f %f %f %f %f\n", sz, dir, min_x, max_x, min_y, max_y, x, y);
  }
  
  template<typename Archive>
  void save(Archive &ar, const unsigned int) const {
    assert(next != NULL); //only graphs alreay processed by tsp should be sent

    int i = 0;
    const tree *p = this;
    
    do {
      i++;
      p = p->next;
    } while( p != this);
    
    ar << i;
    
    double *buf = (double *)malloc(sizeof(double) * 2 * i);
    assert(buf != NULL);
    
    double *bufptr = buf;
    
    
    p = this;
    do {
      *bufptr++ = p->x;
      *bufptr++ = p->y;
      p = p->next;
    } while( p != this);
 
    ar & boost::serialization::make_array(buf, 2 * i);
    
    free(buf);
    
  }
  
  template<typename Archive>
  void load(Archive &ar, const unsigned int) {
    int tot, i;
    ar >> tot;
    
    double *buf = (double *)malloc(sizeof(double) * 2 * tot);
    assert(buf != NULL);
    
    ar & boost::serialization::make_array(buf, 2 * tot);
    
    tree *m = new tree[tot];
    tree *this_p = this; // just in case
    pr_pfor(i, 0, tot, nthreads * tasks_per_thread,
      tree& p = m[i];
      p.x = buf[2 * i];
      p.y = buf[2 * i + 1];
      p.next  = (i == (tot - 1)) ? this_p : (m + (i + 1));
      p.prev  = m + ((i ? i : tot) - 1);
      p.left  = NULL;
      p.right = NULL;
    );
    *this = m[0]; //m[0] becomes a waste
    m[1].prev = this;
    
    free(buf);
  }
  
  BOOST_SERIALIZATION_SPLIT_MEMBER();

};

TRANSMIT_BY_CHUNKS(tree);

typedef tree* Tree;

// Should optimize a bit
//BOOST_CLASS_IMPLEMENTATION(tree, boost::serialization::object_serializable);

/* Builds a 2D tree of n nodes in specified range with dir as primary
 axis (0 for x, 1 for y) */
//void build_tree(Tree root, int n, int dir, double min_x, double max_x, double min_y, double max_y);
/* Compute TSP for the tree t -- use conquer for problems <= sz */

Tree conquer(Tree t);
Tree merge(Tree a, Tree b, Tree t);
Tree makelist(Tree t);
void reverse(Tree t);
double distance(Tree a, Tree b);
int dealwithargs(int argc, char** argv);

struct TreeInfo: public dpr::DInfo<Tree, 2> {
	static int sz;

        TreeInfo() : dpr::DInfo<Tree, 2>(nthreads * tasks_per_thread) {}

	bool is_base(const Tree t) const {
		return t->sz <= sz;
	}

	Tree child(int i, const Tree t) const {
                return i ? t->right : t->left ;
	}
};

struct TSPBody : public dpr::EmptyBody<Tree, Tree> {
	Tree base(Tree t) {
		return conquer(t);
	}

	Tree post(Tree t, Tree *results) {
		return merge(results[0], results[1], t);
	}
};

struct TSPBuildBody : dpr::EmptyBody<Tree, void> {
  void pre(Tree t) {
    t->pre();
  }
};
