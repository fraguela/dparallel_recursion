/*
 *         ---- The Unbalanced Tree Search (UTS) Benchmark ----
 *  
 *  Copyright (c) 2010 See AUTHORS file for copyright holders
 *
 *  This file is part of the unbalanced tree search benchmark.  This
 *  project is licensed under the MIT Open Source license.  See the LICENSE
 *  file for copyright and licensing information.
 *
 *  UTS is a collaborative project between researchers at the University of
 *  Maryland, the University of North Carolina at Chapel Hill, and the Ohio
 *  State University.  See AUTHORS file for more information.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "uts.h"
#undef min
#undef max
#include <algorithm>

/***********************************************************
 *                                                         *
 *  Compiler Type (these flags are set by at compile time) *
 *     (default) ANSI C compiler - sequential execution    *
 *                                                         *
 ***********************************************************/

#define PARALLEL         0
#define COMPILER_TYPE    0
#define MAX_THREADS      1
#define LOCK_T           void
#define GET_NUM_THREADS  1
#define GET_THREAD_NUM   0
#define SMEMCPY          memcpy
#define ALLOC            malloc


/***********************************************************
 *  Parallel execution parameters                          *
 ***********************************************************/

#define DEFAULT_MAXSTACKDEPTH 500000
int doSteal   = PARALLEL; // 0 => sequential, don't use work stealing
int chunkSize = 20;       // number of nodes to move to/from shared area

int initialStackSize = DEFAULT_MAXSTACKDEPTH;	// stack size

/***********************************************************
 * StealStack types                                        *
 ***********************************************************/

/* stack of nodes */
struct stealStack_t
{
  int stackSize;     /* total space avail (in number of elements) */
  int workAvail;     /* elements available for stealing */
  int sharedStart;   /* index of start of shared portion of stack */
  int local;         /* index of start of local portion */
  int top;           /* index of stack top */
  int maxStackDepth;                      /* stack stats */ 
  int nNodes, maxTreeDepth;               /* tree stats  */
  int nLeaves;
  Node * stack;       /* addr of actual stack of nodes in local addr space */
};
typedef struct stealStack_t StealStack;

/***********************************************************
 *  UTS Implementation Hooks                               *
 ***********************************************************/

// Return a string describing this implementation
char * impl_getName() {
  char * name[] = {"Sequential C"};
  return name[COMPILER_TYPE];
}


// construct string with all parameter settings
int impl_paramsToStr(char *strBuf, int ind) {
  ind += sprintf(strBuf+ind, "Execution strategy:  ");
  ind += sprintf(strBuf+ind, "   Initial Stack Size: %d\n", initialStackSize);
  ind += sprintf(strBuf+ind, "   Iterative sequential search\n");
  return ind;
}


int impl_parseParam(char *param, char *value) {
  int err = 0;  // Return 0 on a match, nonzero on an error

  switch (param[1]) {
    case 'S':
      initialStackSize = atoi(value); break;
    default:
      err = 1;
      break;
  }

  return err;
}

void impl_helpMessage() {
  printf("   -S  int   set initial stack size\n");
}

void impl_abort(int err) {
  exit(err);
}

/***********************************************************
 *                                                         *
 *  FUNCTIONS                                              *
 *                                                         *
 ***********************************************************/

/* 
 * StealStack
 *    Stack of nodes with sharing at the bottom of the stack
 *    and exclusive access at the top for the "owning" thread 
 *    which has affinity to the stack's address space.
 *
 *    * All operations on the shared portion of the stack
 *      must be guarded using the stack-specific lock.
 *    * Elements move between the shared and exclusive
 *      portion of the stack solely under control of the 
 *      owning thread. (ss_release and ss_acquire)
 *    * workAvail is the count of elements in the shared
 *      portion of the stack.  It may be read without 
 *      acquiring the stack lock, but of course its value
 *      may not be acurate.  Idle threads read workAvail in
 *      this speculative fashion to minimize overhead to 
 *      working threads.
 *    * Elements can be stolen from the bottom of the shared
 *      portion by non-owning threads.  The values are 
 *      reserved under lock by the stealing thread, and then 
 *      copied without use of the lock (currently space for
 *      reserved values is never reclaimed).
 *
 */

/* restore stack to empty state */
void ss_mkEmpty(StealStack *s) {
  s->sharedStart = 0;
  s->local  = 0;
  s->top    = 0;
  s->workAvail = 0;
}

/* fatal error */
void ss_error(char *str) {
  printf("*** [Thread %i] %s\n",0, str);
  exit(4);
}

/* initialize the stack */
void ss_init(StealStack *s, int nelts) {
  int nbytes = nelts * sizeof(Node);

  s->stack = (Node *) ALLOC (nbytes);
  if (s->stack == NULL) {
    printf("Request for %d bytes for stealStack on thread %d failed\n",
           nbytes, 0);
    ss_error("ss_init: unable to allocate space for stealstack");
  }
  s->stackSize = nelts;
  s->nNodes = 0;
  s->maxStackDepth = 0;
  s->maxTreeDepth = 0;
  s->nLeaves = 0;
  ss_mkEmpty(s);
}


/* local push */
void ss_push(StealStack *s, Node *c) {
  if (s->top >= s->stackSize) {
    ss_error("ss_push: overflow");
  }

  memcpy(&(s->stack[s->top]), c, sizeof(Node));
  s->top++;
  s->nNodes++;
  s->maxStackDepth = std::max(s->top, s->maxStackDepth);
  s->maxTreeDepth = std::max(s->maxTreeDepth, c->height);
}

/* local top:  get local addr of node at top */ 
Node * ss_top(StealStack *s) {
  Node *r;
  if (s->top <= s->local)
    ss_error("ss_top: empty local stack");
  r = &(s->stack[(s->top) - 1]);
  return r;
}

/* local pop */
void ss_pop(StealStack *s) {
  if (s->top <= s->local)
    ss_error("ss_pop: empty local stack");
    s->top--;
}
  
/* local top position:  stack index of top element */
int ss_topPosn(StealStack *s)
{
  if (s->top <= s->local)
    ss_error("ss_topPosn: empty local stack");
  return s->top - 1;
}

/* local depth */
int ss_localDepth(StealStack *s) {
  return (s->top - s->local);
}

/*
 *	Tree Implementation      
 *
 */
void initNode(Node * child)
{
  child->type = -1;
  child->height = -1;
  child->numChildren = -1;    // not yet determined
}


void initRootNode(Node * root, int type)
{
  uts_initRoot(root, type);
}


/* 
 * Generate all children of the parent
 *
 * details depend on tree type, node type and shape function
 *
 */
void genChildren(Node * parent, Node * child, StealStack * ss) {
  int parentHeight = parent->height;
  int numChildren, childType;

  numChildren = uts_numChildren(parent);
  childType   = uts_childType(parent);

  // record number of children in parent
  parent->numChildren = numChildren;
  
  // construct children and push onto stack
  if (numChildren > 0) {
    int i, j;
    child->type = childType;
    child->height = parentHeight + 1;

    for (i = 0; i < numChildren; i++) {
      for (j = 0; j < computeGranularity; j++) {
        // TBD:  add parent height to spawn
        // computeGranularity controls number of rng_spawn calls per node
        rng_spawn(parent->state.state, child->state.state, i);
      }
      ss_push(ss, child);
    }
  } else {
    ss->nLeaves++;
  }
}

/* 
 * sequential search of UTS trees using work stealing 
 * 
 *   Note: tree size is measured by the number of
 *         push operations
 */
void parTreeSearch(StealStack *ss) {
  Node * parent;
  Node child;

  /* template for children */
  initNode(&child);

  /* tree search */
  /* local work */
  while (ss_localDepth(ss) > 0) {		
    /* examine node at stack top */
    parent = ss_top(ss);
    if (parent->numChildren < 0){
      // first time visited, construct children and place on stack
      genChildren(parent,&child,ss);
    } else {
      // second time visit, process accumulated statistics and pop
      ss_pop(ss);
    }
  }
  /* tree search complete ! */
}

// display search statistics
void showStats(double elapsedSecs, StealStack *ss) {
  int i;
  int tnodes = 0, tleaves = 0, trel = 0, tacq = 0, tsteal = 0, tfail= 0;
  int mdepth = 0, mheight = 0;
  double twork = 0.0, tsearch = 0.0, tidle = 0.0, tovh = 0.0, tcbovh = 0.0;

  tnodes  = ss->nNodes;
  tleaves = ss->nLeaves;
  mheight = std::max(mheight, ss->maxTreeDepth);

  if (trel != tacq + tsteal) {
    printf("*** error! total released != total acquired + total stolen\n");
  }
    
  uts_showStats(GET_NUM_THREADS, chunkSize, elapsedSecs, tnodes, tleaves, mheight);
}

/*  Main() function for: Sequential
 */
int main(int argc, char *argv[]) {
  Node root;

  /* determine benchmark parameters (all PEs) */
  uts_parseParams(argc, argv);

  double t1, t2, et;
  StealStack * ss;    

  /* initialize stealstacks */
  ss = (StealStack *) ALLOC (sizeof(StealStack));
  ss_init(ss, initialStackSize);
  
  initRootNode(&root, type);
  ss_push(ss, &root);

  t1 = uts_wctime();
  parTreeSearch(ss);
  t2 = uts_wctime();
  et = t2 - t1;

  showStats(et, ss);

  return 0;
}

