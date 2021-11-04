/* Tutorial on Reverse Search                   */
/* Section 4: C Implementation for Permutations */
/* Solution to Ex 2.2 topological sorts         */

/* Input: enter number n of vertices and m edges  */
/*     followed by m pairs i j with 1 <=i<j<=n    */ 
/*     this implies that 1,2,...,n is a topsort   */
/*     vertices are labelled 1,2,...,n            */

/* This code is derived from atopsorts.c to allow multithreading */
/* using the mts wrapper                                         */

/* For single thread use compile with:                           */
/* gcc -O3 -o btopsorts mtslib.c btopsorts.c                     */

/* Usage: btopsorts [file] [-countonly] [-maxnodes n] [-maxd d] [-prune 0/1] [-debug] */

/* pm6 is a sample input file that generates 15 permutations                    */

/* Changes include:
   1. mts.h and mtslib.c required for compilation 
   2. mts.h contains definitions of Node and Data structures      
   3. Node keeps all data for the current tree node, initialized to root
   4. Data is a string representing the raw input file (or stdin)
   5. mtslib.c contains get_input to convert input to Data for internal use 
   6. MTS compile switch allows alternative output for multithreading
   7. -prune 0 does not return leaves with unexplored
   8. -prune 1 does not return leaves or single child nodes with unexplored
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "mts.h"
#include <unistd.h>
#ifndef MTS
#include <sys/time.h>
#endif


#define TRUE 1
#define FALSE 0

#ifdef MTS
#define tsprintf stream_printf
#define tstream mts_stream
#else
#define tsprintf fprintf
#define tstream FILE
#endif

typedef long Perm[100];

/* bts_data is built from the input and can be user modified  */
/* except where mentioned                                     */

struct bts_data {
	int n;                 /* number of nodes and edges in graph */
	int m;
	int A[100][100];       /* the graph */
        int countonly;         /* TRUE means nodes are counted but not output */
        int prune;             /* mark unexplored only if nchild > prune */
        tstream *output;       /* do not remove */
        tstream *err;          /* do not remove */
};

char name[] = "btopsorts";

const mtsoption bts_options[] = {
 {"-countonly", 0},     /* -countonly option has no parameters */
 {"-prune", 1},         /* -prune option has one parameter */
};
const int bts_options_size = 2;

/*************************************************************************/
/* Required routines for multithreading:                                 */
/*  bts_init, get_root, put_output freebdata                             */
/*************************************************************************/

void bts_finish(bts_data *bdata, shared_data *sdata, int size, int checkpointing) {
  return;
}

/* bts_init is executed once for each thread                             */
/* multithreading can be ignored here except for using                   */
/* fscanf(f,..) to read the input and the supplied                       */
/* tsprintf for output to either stdout (or file) and stderr(or file)    */
/* to get output only once use if(proc_no ==0 ) ....                     */
/* note that file pointer f points to an input string built in get_input */

bts_data *bts_init(int argc, char **argv, Data *in, int proc_no) {
	int i, j, k, n,m;
	FILE *f;

	bts_data *b = (bts_data *)malloc(sizeof(bts_data));

        b->countonly=FALSE;   
        b->prune=-1;           /* no pruning of unexplored nodes */


/* process command line arguments to btopsorts        */
/* input file if any is handled by get_input          */
/* some error checking may be nice !                  */

        i=1;
        while (i<argc) {
          if (strcmp(argv[i],"-countonly") == 0)
             b->countonly=TRUE;
          if (strcmp(argv[i],"-prune") == 0)
             b->prune = atol(argv[++i]);
          i++;
         }
	f = open_string(in->input);
#ifdef MTS
	b->output = open_stream(MTSOUT);
	b->err = open_stream(MTSERR);
#else
	b->output = stdout;
	b->err = stderr;
#endif

	i = fscanf(f, "%d %d\n", &b->n, &b->m);
    if (i == EOF ) {
	   tsprintf(b->err, "no input found\n");
	   fclose(f);
#ifdef MTS
	   close_string();
#endif
	   return NULL;
	}

        n=b->n;
        m=b->m;

/* read input data containing the input graph */

	for (i=1; i<=n; i++)
		for (j=1; j<=n; j++)
			b->A[i][j] = 0;

	for (i=0; i<m; i++) {
		fscanf(f, "%d %d\n", &j, &k);
        if(j<1 || j>n || k< 1 || k > n || j >= k ) {
		   tsprintf(b->err, "bad edge ij: required 1<=i<j<=n\n");
		   fclose(f);
#ifdef MTS
		   close_string();
#endif
		   return NULL;
		}
		b->A[j][k] = 1;
	}

	
	fclose(f);
#ifdef MTS
	close_string();
#endif

	return b;
}                         /* end of bts_init      */

/* the root node of the tree search is built here */

Node *get_root(bts_data *b) {	/* return a node which is the root of the search tree  */
   int i,n;
   Node *root = (Node *)malloc (sizeof(Node));

   n=b->n;
   root->vlong = (long *)malloc(sizeof(long)*(n+1));
   root->size_vlong = n+1;
   root->vint = NULL;
   root->size_vint =0;
   root->vchar = NULL;
   root->size_vchar = 0;
   root->vfloat = NULL;
   root->size_vfloat = 0;
   root->depth = 0;
   root->unexplored = FALSE;

   for (i = 1; i <= n; i++)
            root->vlong[i] = i;

   if (b->countonly == FALSE)
        put_output(b,root);  /* first output ! */
   return root;
}


void put_output (bts_data *b, Node *treenode) {
 int i; 
 /* usleep(1);        if you want to kill time */
 if(!b->countonly) {
   for (i = 1; i <= b->n; i++)
      tsprintf (b->output," %ld", treenode->vlong[i]);
   tsprintf (b->output, " d=%ld",treenode->depth);
#ifndef MTS
   if (treenode->unexplored)
       tsprintf (b->output, " *unexplored");
#endif
   tsprintf (b->output, "\n");
 }
#ifdef MTS
 if (treenode->unexplored)
   return_unexp(treenode);
#endif
}

void freebdata(bts_data *b) {
	free(b);
}


void copy (bts_data *b, Perm w, Perm v) {
  int i;
  int n = b->n;

  for (i = 1; i <= n; i++)
    w[i] = v[i];
}

int equal (bts_data *b, Perm w, Perm v) {
  int i, n=b->n;
  for (i = 1; i <= n; i++)
    if (w[i] != v[i])
      return FALSE;
  return TRUE;
}


void swap(Perm v, int j) {
  int t;
  t = v[j];
  v[j] = v[j + 1];
  v[j + 1] = t;
}

int f (bts_data *b, Perm v) {			/* local search function */
  int i=1, n=b->n;
  while ( i < n && v[i] <= v[i + 1] ) i++;
  if (i < n) swap (v, i);
  return i;
}

int reverse(bts_data *b, int s, Perm v, int i) {
  if(b->A[v[i]][v[i+1]]==1) return (FALSE);  // v[j]v[j+1] is an edge
  return (((i <= s-1) || ((i == (s + 1)) && (s <= ((b->n) - 2)) && (v[s] < v[s+2]))) ? TRUE : FALSE);
}

int kids(bts_data *b, int idx, Perm v) {	/* return true if v has > 1 children */
  int child =0;
  int i;

  for (i = 1; i <= (b->n)-1; i++) {
    if (reverse(b, idx, v, i)) {
      child++;
      if(child > b->prune)
        return TRUE;
    }
  }
  return FALSE;
}

long bts(bts_data *b, Node *treenode, long max_depth, long max_nodes, shared_data *sdata, int rank, int size) {		/* budgeted reverse search code for topsorts */
  long    *v, *depth;
  int *unexplored;
  int maxdeg;
  long init_depth;
  int j=0; 
  long count=0;
  int n = b->n;

  v=&treenode->vlong[0]; 
  depth=&treenode->depth;
  unexplored=&treenode->unexplored;

  maxdeg=n-1;
  init_depth= *depth;      /* depth of root of this subtree */

  do {
#ifdef MTS
      open_outputblock(); /* demo: don't flush inside block */
#endif
      *unexplored = FALSE;
      while (j < maxdeg && !(*unexplored) ) {
         j++;
         int idx;
         for(idx = 1; (idx < n) && (v[idx] < v[idx+1]); idx++);
         if (reverse(b, idx, v, j)) {                        /* forward step */
           swap(v, j);
           (*depth)++;
           count++;
           if (count >= max_nodes || (*depth) == max_depth)
              if(b->prune < 0 || kids(b,idx,v))    /* nodes with degree > b->prune are unexplored */
                *unexplored=TRUE;
           put_output(b, treenode);
           j = 0;
         }
      }   /* end while */

     if(*depth > init_depth) {                        /* backtrack step */
        j = f(b, v);
        (*depth)--;
     }
#ifdef MTS
     close_outputblock(); /* demo: okay to flush outside block */
#endif
  } while( (*depth) > init_depth || j < maxdeg);
  return count;
}


#ifndef MTS 
int main(int argc, char **argv) { /* generate top sorts */
  Data *in;
  Node *root;
  bts_data *b;
  long count;
  long max_depth;
  long max_nodes=LONG_MAX;       /* replacing maxcobasis */
  int i;
  struct timeval t0, t1, t;



  printf("\n*btopsorts 2016.6.3:");
  printf(TITLE);
  printf(VERSION);
  printf("\n");

/* The following is essential set up for single/multi thread tree search                 */

  in =   malloc (sizeof(Data));
  get_input(argc,argv,in);         /* converts input data to a string Data *in      */
  b = bts_init(argc,argv,in,0);    /* build node data structure bts_data *b from in */
  if (b == NULL)
	return 0;
  root=get_root(b);                /* build the tree root from bts_data *b          */

/* End of essential setup */

  max_depth=LONG_MAX;
  max_nodes=LONG_MAX;

  for (i=1; i < argc; i++) {
        if (strcmp(argv[i],"-maxd") == 0)
              max_depth = atoi(argv[i+1]);
        if (strcmp(argv[i],"-maxnodes") == 0)
               max_nodes = atoi(argv[i+1]);
  }

/* the tree search is now initiated from root */

  gettimeofday(&t0, NULL);
  count = bts(b, root,max_depth,max_nodes,NULL,0,1);
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);

/* final totals */

  tsprintf (b->err,"\nnumber of permutations=%ld\n", count+1); /* count the root */
  tsprintf (b->err,"compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  free(in);
  free(root);
  freebdata(b);
  return 0;
}
#endif
