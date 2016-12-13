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
/// \file     tsp.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */

#include "tsp.h"
#include <mpi.h>
#include <cstdlib>
#include <cmath>


extern void print_list(Tree t);
//extern void print_tree(Tree t);

/* communications */

void send(int dest_rank, int tag, Tree t)
{
  assert(t->next != NULL); //only graphs alreay processed by tsp should be sent
  
  int i = 0;
  const tree *p = t;
  
  do {
    i++;
    p = p->next;
  } while( p != t);
  
  double *buf = (double *)malloc(sizeof(double) * 2 * i);
  assert(buf != NULL);
  
  double *bufptr = buf;
  
  p = t;
  do {
    *bufptr++ = p->x;
    *bufptr++ = p->y;
    p = p->next;
  } while( p != t);
  
  MPI_Send(buf, 2 * i, MPI_DOUBLE, dest_rank, tag, MPI_COMM_WORLD);
  
  free(buf);
}

/** end communications */

/* Find Euclidean distance from a to b */
double distance(Tree a, Tree b) {
	double ax, ay, bx, by;

	ax = a->x;
	ay = a->y;
	bx = b->x;
	by = b->y;
	return (sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)));
}

/* sling tree nodes into a list -- requires root to be tail of list */
/* only fills in next field, not prev */
Tree makelist(Tree t) {
	Tree left, right;
	Tree tleft, tright;
	Tree retval = t;

	if (!t)
		return NULL;

	left = makelist(t->left); /* head of left list */
	right = makelist(t->right); /* head of right list */

	if (right) {
		retval = right;
		tright = t->right;
		tright->next = t;
	}
	if (left) {
		retval = left;
		tleft = t->left;
		tleft->next = (right) ? right : t;
	}
	t->next = NULL;

	return retval;
}

/* reverse orientation of list */
void reverse(Tree t) {
	Tree prev, back, next, tmp;

	if (!t)
		return;
	prev = t->prev;
	prev->next = NULL;
	t->prev = NULL;
	back = t;
	tmp = t;
	for (t = t->next; t; back = t, t = next) {
		next = t->next;
		t->next = back;
		back->prev = t;
	}
	tmp->next = prev;
	prev->prev = tmp;
	/*printf("REVERSE result\n");*/
	/*print_list(tmp);*/
	/*printf("End REVERSE\n");*/
}

/* Use closest-point heuristic from Cormen Leiserson and Rivest */
Tree conquer(Tree t) {
	Tree cycle, tmp, min, prev, next, donext;
	double mindist, test;
	double mintonext, mintoprev, ttonext, ttoprev;

	if (!t)
		return NULL;
	t = makelist(t);

	cycle = t;
	t = t->next;
	cycle->next = cycle;
	cycle->prev = cycle;

	for (; t; t = donext) { /* loop over remaining points */
		donext = t->next; /* value won't be around later */
		min = cycle;
		mindist = distance(t, cycle);
		for (tmp = cycle->next; tmp != cycle; tmp = tmp->next) {
			test = distance(tmp, t);
			if (test < mindist) {
				mindist = test;
				min = tmp;
			} /* if */
		} /* for tmp... */
		next = min->next;
		prev = min->prev;
		mintonext = distance(min, next);
		mintoprev = distance(min, prev);
		ttonext = distance(t, next);
		ttoprev = distance(t, prev);
		if ((ttoprev - mintoprev) < (ttonext - mintonext)) {
			/* insert between min and prev */
			prev->next = t;
			t->next = min;
			t->prev = prev;
			min->prev = t;
		} else {
			next->prev = t;
			t->next = next;
			min->next = t;
			t->prev = min;
		}
	} /* for t... */
	return cycle;
}

/* Merge two cycles as per Karp */
Tree merge(Tree a, Tree b, Tree t) {
	//t->left = t->right = a->left = a->right = b->left = b->right = NULL;

	Tree min, next, prev, tmp;
	double mindist, test, mintonext, mintoprev, ttonext, ttoprev;
	Tree n1, p1, n2, p2;
	double tton1, ttop1, tton2, ttop2;
	double n1ton2, n1top2, p1ton2, p1top2;
	int choice;
  
        //printf("Merging [%p]\n", t);
  
	/* Compute location for first cycle */
	min = a;
	mindist = distance(t, a);
	tmp = a;
	for (a = a->next; a != tmp; a = a->next) {
		test = distance(a, t);
		if (test < mindist) {
			mindist = test;
			min = a;
		} /* if */
	} /* for a... */
	next = min->next;
	prev = min->prev;
	mintonext = distance(min, next);
	mintoprev = distance(min, prev);
	ttonext = distance(t, next);
	ttoprev = distance(t, prev);
	if ((ttoprev - mintoprev) < (ttonext - mintonext)) {
		/* would insert between min and prev */
		p1 = prev;
		n1 = min;
		tton1 = mindist;
		ttop1 = ttoprev;
	} else { /* would insert between min and next */
		p1 = min;
		n1 = next;
		ttop1 = mindist;
		tton1 = ttonext;
	}

	/* Compute location for second cycle */
	min = b;
	mindist = distance(t, b);
	tmp = b;
	for (b = b->next; b != tmp; b = b->next) {
		test = distance(b, t);
		if (test < mindist) {
			mindist = test;
			min = b;
		} /* if */
	} /* for tmp... */
	next = min->next;
	prev = min->prev;
	mintonext = distance(min, next);
	mintoprev = distance(min, prev);
	ttonext = distance(t, next);
	ttoprev = distance(t, prev);
	if ((ttoprev - mintoprev) < (ttonext - mintonext)) {
		/* would insert between min and prev */
		p2 = prev;
		n2 = min;
		tton2 = mindist;
		ttop2 = ttoprev;
	} else { /* would insert between min and next */
		p2 = min;
		n2 = next;
		ttop2 = mindist;
		tton2 = ttonext;
	}

	/* Now we have 4 choices to complete:
	 1:t,p1 t,p2 n1,n2
	 2:t,p1 t,n2 n1,p2
	 3:t,n1 t,p2 p1,n2
	 4:t,n1 t,n2 p1,p2 */
	n1ton2 = distance(n1, n2);
	n1top2 = distance(n1, p2);
	p1ton2 = distance(p1, n2);
	p1top2 = distance(p1, p2);

	mindist = ttop1 + ttop2 + n1ton2;
	choice = 1;

	test = ttop1 + tton2 + n1top2;
	if (test < mindist) {
		choice = 2;
		mindist = test;
	}

	test = tton1 + ttop2 + p1ton2;
	if (test < mindist) {
		choice = 3;
		mindist = test;
	}

	test = tton1 + tton2 + p1top2;
	if (test < mindist)
		choice = 4;

	/*chatting("p1,n1,t,p2,n2 0x%x,0x%x,0x%x,0x%x,0x%x\n",p1,n1,t,p2,n2);*/
	switch (choice) {
	case 1:
		/* 1:p1,t t,p2 n2,n1 -- reverse 2!*/
		/*reverse(b);*/
		reverse(n2);
		p1->next = t;
		t->prev = p1;
		t->next = p2;
		p2->prev = t;
		n2->next = n1;
		n1->prev = n2;
		break;
	case 2:
		/* 2:p1,t t,n2 p2,n1 -- OK*/
		p1->next = t;
		t->prev = p1;
		t->next = n2;
		n2->prev = t;
		p2->next = n1;
		n1->prev = p2;
		break;
	case 3:
		/* 3:p2,t t,n1 p1,n2 -- OK*/
		p2->next = t;
		t->prev = p2;
		t->next = n1;
		n1->prev = t;
		p1->next = n2;
		n2->prev = p1;
		break;
	case 4:
		/* 4:n1,t t,n2 p2,p1 -- reverse 1!*/
		/*reverse(a);*/
		reverse(n1);
		n1->next = t;
		t->prev = n1;
		t->next = n2;
		n2->prev = t;
		p2->next = p1;
		p1->prev = p2;
		break;
	}
	return t;
}

/* Compute TSP for the tree t -- use conquer for problems <= sz */
Tree tsp(Tree t, int sz) {
  Tree leftval, rightval;
  
  if (t->sz <= sz)
    return conquer(t);
  
  leftval = _Cilk_spawn tsp(t->left, sz);
  rightval = tsp(t->right, sz);
  _Cilk_sync;

  return merge(leftval, rightval, t);
}

Tree *parent_partition_row;

void get_parents_for_level(Tree t, const int bottom, Tree * const parent_partition_row, int& pos)
{
  if ((t->sz / 2) == bottom ) {
    parent_partition_row[pos++] = t;
  } else {
    get_parents_for_level(t->left, bottom, parent_partition_row, pos);
    get_parents_for_level(t->right, bottom, parent_partition_row, pos);
  }
}

Tree tsp(int rank, int nprocs, Tree t, int sz) {
  
  if (nprocs == 1) {
    t = tsp(t, sz);
  } else {
    
    //assert(sz <= bottom_n); //conquer can only have happened at bottom_n or below
    
    //printf("parallel from %d while > %d\n", bottom_n, par_level);
    
    for (int i = 0; i < myleaves; i++) {
      partition_row[myfirst_leaf + i] = tsp(partition_row[myfirst_leaf + i], sz);
    }
    
    //printf("r=%d myl=%d f=%d:\n", rank, myleaves, myfirst_leaf);
    
    if (rank) {
      for (int i = 0; i < myleaves; i++) {
        //print_list(partition_row[myfirst_leaf + i]); printf("Bye %d\n", rank);
        send(0, myfirst_leaf + i, partition_row[myfirst_leaf + i]);
      }
      t = NULL;
    } else {
      Tree parent_partition_row[nleaves / 2];

      const int ntasks = nthreads * tasks_per_thread;

      for (int i = 0; i < (nleaves - myleaves); i++) {
        MPI_Status st;
        int len;
        
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
        const int from_rank = st.MPI_SOURCE;
        const int tag = st.MPI_TAG;
        MPI_Get_count(&st, MPI_DOUBLE, &len);
        //printf("Recv %d doubles from %d with tag=%d\n", len, st.MPI_SOURCE, st.MPI_TAG);
        
        double * buf = (double *)malloc(sizeof(double) * len);
        
        MPI_Recv(buf, len, MPI_DOUBLE, from_rank, tag, MPI_COMM_WORLD, &st);

        const int chunk_size = (len + ntasks - 1) / ntasks;

        len /= 2;
        
        _Cilk_spawn [buf, len, tag, chunk_size]
        {
          Tree data = new tree[len];
          
          _Cilk_for(int j=0; j < len; j+=chunk_size) {
            const int limit = std::min(j + chunk_size, len);
            for(int i=j; i < limit; i++) {
              tree& p = data[i];
              p.x = buf[2 * i];
              p.y = buf[2 * i + 1];
              p.next  = data + ((i == (len - 1)) ? 0 : (i + 1));
              p.prev  = data + ((i ? i : len) - 1);
              p.left  = NULL;
              p.right = NULL;
            }
          }
          free(buf);
          partition_row[tag] = data;
        } ();
      }
      
      _Cilk_sync;
      
      //for (int i = 0; i < nleaves;i++) print_list(partition_row[i]); return 0;
      
      //set_par_level(t->sz);
      
      
      //printf("parallel from %d while > %d\n", t->sz, par_level);
      int cur_step = 2;
      for (int cur_n = bottom_n; cur_n < t->sz; cur_n *= 2) {
        int cur_leaves = 0;
        get_parents_for_level(t, cur_n, parent_partition_row, cur_leaves);
        //printf("%d parents for leaves at sz %d step %d\n", cur_leaves, cur_n, cur_step);
        _Cilk_for(int i = 0; i < cur_leaves; i++) {
          partition_row[cur_step * i] = merge(partition_row[cur_step * i], partition_row[cur_step * i + cur_step/2], parent_partition_row[i]);
        }
        cur_step *= 2;
      }
/*
#pragma omp parallel
#pragma omp single
      t = tsp_top(t, sz, 0);
*/
    }
  }
  
  return t;
}
