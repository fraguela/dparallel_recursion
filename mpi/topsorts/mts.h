/* mts.h
 * Header for mts.
 */

#define TITLE "mtslib "
#define VERSION "v.0.1 2016.9.26"
#define AUTHOR "*Copyright (C) 2016, David Avis, Charles Jordan"

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include <stdio.h>

#ifndef MTSH
#define MTSH

typedef struct  node_v {
	long *vlong;		/* tree node longs (for user) */
	unsigned int size_vlong;/* number of longs in vlong */
        int *vint;       	/* tree node ints (for user)  */
	unsigned int size_vint;	/* number of ints in vint */
	char *vchar;		/* tree node chars (for user) */
	unsigned int size_vchar;/* number of chars in vchar */
	float *vfloat;		/* tree node floats (for user) */
	unsigned int size_vfloat;/*number of floats in vfloat */

	int unexplored;	/* this subtree is unexplored */
	long depth;	/* depth of v in tree */
	struct node_v *next;	/* for list of unexplored nodes */
} Node;

typedef struct data_v {
	char *input;			/* data needed for input */
	unsigned int input_size; 	/* number of chars in input (incl \0)*/
} Data;

struct mts_stream;
typedef struct mts_stream mts_stream;

typedef struct sdata_v {
	long *sdlong;		  /* shared data longs */
	unsigned int size_sdlong; /* number of longs in sdlong */
	int *sdint;		  /* shared data ints */
	unsigned int size_sdint;  /* number of ints in sdint */
	char *sdchar;		  /* shared data chars */
	unsigned int size_sdchar; /* number of chars in sdchar */
	float *sdfloat;		  /* shared data floats */
	unsigned int size_sdfloat;/* number of floats in sdfloat */
	int modified;  /* boolean, 0: false 1:true */
} shared_data;

/* mts provides the following. Flushing is optional, mts will
 * flush after bts returns.  Flushing may happen in stream_printf.
 * 
 * Flushing will not happen 'inside' an outputblock -- if you have a
 * block of output that must stay together in that order, call
 * open_outputblock() first, and close_outputblock() after finishing
 * the block.  All open outputblocks are closed when bts returns.
 * Nesting outputblocks is fine -- there will be no flush until
 * all outputblocks have been closed.  
 */
/* note: you are allowed one stream with dest MTSOUT and one MTSERR, 
 * opening >1 of either results in unusual behavior that is likely to change
 */
#define MTSOUT 1
#define MTSERR 2
mts_stream *open_stream(int dest); /* MTSOUT: output, MTSERR: stderr */
int stream_printf(mts_stream *str, const char *fmt, ...);
void flush_stream(mts_stream *str);
void close_stream(mts_stream *str); /* closes a stream returned by open_stream*/
void open_outputblock(void);
void close_outputblock(void);
char *mts_filetostr(FILE *); /* reads file contents as \0-delim string,
			      * caller responsible for fclose and freeing ret */
FILE *open_string(const char *string); /* for reading with file pointer */
void close_string(void);

/* cleanstop is available only to bts() and may not be called from
 * bts_init() etc.
 */
/* cleanstop: signal master to exit cleanly when possible.
 * suggest a checkpoint (checkpoint!=0) or not (checkpoint==0).
 * returns to caller, which must return from bts(...).
 */
void cleanstop(int checkpoint);
/* emergencystop: something has gone wrong -- kill all processes
 * if possible (including this one) after printing the message.
 * doesn't return.
 */
void emergencystop(char *msg);

/* and the following, to report a single unexplored node
 * (node->next is ignored) */
void return_unexp(const Node *node);

/* implementing the following allows re-use for each application.
 */

/* given command-line argc/argv, initialize root and in.
 * Application is responsible for allocating root->v and
 * in->input.
 * Return 0 if failed, 1 if okay to continue */

int get_input(int argc, char **argv, Data *in);

/* given in, initialized by get_input, initialize internal structures */
/* the application should define bts_data however it likes. mts doesn't
 * touch internals.
 */
/* application must define struct bts_data.
 */
struct bts_data;
typedef struct bts_data bts_data;
bts_data *bts_init(int argc, char **argv, Data *in, int proc_no);
/* bts_finish: do any final prints/etc. This shared_data is guaranteed to
 * be the final set.  checkpointing true if we interrupted and are checkpointing
 * Note: not called if emergencystop(), we fail to start cleanly, etc */
void bts_finish(bts_data *, shared_data *, int size, int checkpointing);
Node *get_root(bts_data *);
void put_output(bts_data *b, Node * treenode);
extern char name[]; /* a short C-string that names your program */

/* free the bts_data returned by bts_init */
void freebdata(bts_data *bdata);

/* do budgeted tree search in input, starting at treenode
 * mts will free treenode->v and treenode later
 * note: modifying data may result in getting the modified copy
 * in the future -- application should be able to start from any
 * copy of data and any treenode that mts has seen in this process..
 *
 * sdata[0] ... sdata[size-1] gives shared auxiliary data.  sdata[rank] belongs
 * to you, and the others to others.  likely no process will be called with rank
 * 0 or 1, but this may change.
 *
 * if the application updates sdata[rank] and sets sdata[rank].modified = 1,
 * then after mts returns it will be queued to eventually get sent to other processes.
 * if you update sdata[rank] and set sdata[rank].modified = 1 again in another call,
 * this may or may not replace the first update before other processes have a chance to
 * see it.  the contents of sdata[0]...sdata[size-1] is unlikely to be the same on
 * all processes, it is updated on each process when convenient.  the overall search
 * may end with sdata updates still pending.  the intent is to provide a mechanism to
 * share learnt clauses, etc.
 * if the application updates other sdata entries, this may or may not overwritten and
 * will not be sent to other processes.  sdata[i].modified is set to 0 if this process has
 * already seen the current copy of sdata[i] (ie mts hasn't changed it since the last bts call).
 *
 * large and/or frequently-updated shared_data may affect performance.
 *
 * many applications can completely ignore sdata, rank, and size.
 * for all sdata[i], mts will free(sdata[i].data) when exiting.
 */
long bts(bts_data *data, Node *treenode, long max_depth, long max_nodes, shared_data *sdata, int rank, int size);


typedef struct {
	char *optname; /* option name */
	int num_args;  /* number of parameters */
} mtsoption;

/* The application must define an array bts_options giving the
 * names and number of parameters for its options.
 * These should not conflict with builtin mts options (e.g. -hist)
 * See the sample applications for an example.
 * Setting bts_options_size to 0 is fine if there are no commandline
 * options.
 */

extern const mtsoption bts_options[];
extern const int bts_options_size; /* number of options in bts_options */

#endif 
