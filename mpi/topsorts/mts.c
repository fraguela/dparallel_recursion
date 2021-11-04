/* generic MPI driver for tree search 
   example usage: 
    mpicc -DMTS -o btopsortsmts btopsortsmts.c mts.c 
    mpirun -np 10 ./btopsortsmts
*/

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

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include "mts.h"

const mtsoption mts_options[] = {	/* external linkage for */
	{"-maxd", 1},			/* curious applications */
	{"-maxnodes", 1},
	{"-scale", 1},
	{"-lmin", 1},
	{"-lmax", 1},
	{"-maxbuf", 1},
	{"-freq", 1},
	{"-hist", 1},
	{"-checkp", 1},
	{"-stop", 1},
	{"-restart", 1}
};
const int mts_options_size = 11;

/* default parameters, can override on commandline */
static long max_nodes = 5000;
static long maxd = 2;	/* used below lmin*/
static int scale = 40;	/* used above lmax */
static int lmin = 1;	/* 0 means use lmin=lmax */
static int lmax = 3; 
static int maxbuf = 1024*1024; /* max number of bytes to buffer
			 	* before trying to flush a stream
			 	*/
static struct timeval tstart;

static char *fn_hist = NULL;
static FILE *hist = NULL;
static char *fn_freq = NULL;
static FILE *freq = NULL;
static char *fn_checkp = NULL;
static char *fn_stop = NULL;
static char *fn_restart = NULL;

typedef struct mbuff {
	MPI_Request *req;
	void **buf;
	unsigned int *sizes;
	MPI_Datatype *types;
	int *tags;
	int num;
	int target;
	int queue;
	long aux;
	int aux2;
	struct mbuff *next;
} mts_buff;

static int rank, size;
static int tagub; /* upper bound on MPI_TAG */

static int mytag = 100;

static Data data;
static bts_data *bdata = NULL;
static Node *unexp = NULL;
static mts_buff *outbuff = NULL;

/* prototypes for mts.c -- ignore */
int mts_master(int, char **);
int mts_init(int *, char **);
void send_data(int);
void recv_data(Data *);
Node *send_node(int, Node *, int, mts_buff **, shared_data *, long **);
mts_buff *recv_node(Node *, long *, long *, int, mts_buff *);
void return_unexplored(Node *);
mts_buff *get_unexplored(int, unsigned long *, mts_buff *);
Node *proc_unexplored(mts_buff *, Node *, int *, long *);
mts_buff *mts_checkunexp(mts_buff *, Node **, int *, long *, int *);
mts_buff *mts_cleansdbufs(mts_buff *, shared_data *, long **);
mts_buff *mts_cleanbufs(mts_buff *head);
void mts_procsdata(mts_buff *, shared_data *, long **);
void send_output(char *, int);
int mts_consumer(void);
mts_buff *mtscon_getincoming(int *, int, mts_buff *);
mts_buff *mtscon_checkincoming(mts_buff *);
int mts_worker(int, char **);
void bad_args(int, char **);
int removearg(int *argc, char **argv, int i);
void init_files(void);
void do_histogram(struct timeval *,struct timeval *, int, int *, int, long);
mts_buff *mts_addsdbuff(long *, mts_buff *, int);
void send_sdata(int, shared_data *, long, int);
void freembuff(mts_buff *);
int check_stop(void);
Node *master_restart(int *, shared_data *, long **);
void master_checkpoint(Node *, shared_data *, int, long **);

struct mts_stream { char *buf; int buflen; int strlen; int dest; };

static mts_stream streams[2] = {{NULL, 0, 0, 0}}; /* OUT, ERR */

static unsigned int outblock;

#ifdef DEBUG
#define mts_debug(a) printf a
#else
#define mts_debug(a)
#endif

mts_stream *open_stream(int dest)
{
	mts_stream *str;
	if (dest == MTSOUT)
		str = streams;
	else if (dest == MTSERR)
		str = streams+1;
	else
		return NULL;
	str->dest = dest;
	str->buf = (char *)malloc(sizeof(char)*256);
	str->buf[0] = '\0';
	str->buflen = 256;
	str->strlen = 0;
	return str;
}

/* need to be careful and flush only if ending with newline ? */
/* for now stream_printf doesn't flush */
int stream_printf(mts_stream *str, const char *fmt, ...)
{
	int newlen;
	char *newbuf;
	va_list args;

	va_start(args, fmt);

	/* need a buffer of size newlen+1 ('\0') */
	newlen = str->strlen + vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	va_start(args, fmt);

	if (str->buflen > newlen)
	{
		/* current buffer big enough */
		vsprintf(str->buf + str->strlen, fmt, args);
		va_end(args);
		str->strlen = newlen;
		return 1;
	}

	/* buffer too small, double needed size +1 for '\0' in case newlen==0*/
	/* TODO take care of overflow */
	newbuf = (char *)realloc(str->buf, sizeof(char)*((newlen<<1)+1));
	if (newbuf == NULL)
	{
		va_end(args);
		return 0; /* failed */
	}
	str->buf = newbuf;
	vsprintf(str->buf + str->strlen, fmt, args);
	va_end(args);
	str->buflen = (newlen<<1)+1;
	str->strlen = newlen;
	/* if buffer is big, ends in newline, and okay to flush -- do so */
	if (str->strlen > maxbuf && outblock==0 && 
	    str->buf[str->strlen-1]=='\n')
		flush_stream(str);
	return 1;
}

void open_outputblock(void)
{
	outblock++;
}

void close_outputblock(void)
{
	if (outblock>0)
		outblock--;
}

void flush_stream(mts_stream *str)
{
	if (str->buf == NULL)
		return;
	send_output(str->buf, str->dest);
	str->buf=NULL;
	str->strlen = 0;
	str->buflen = 0;
}

void close_stream(mts_stream *str)
{
	free(str->buf);
	str->buf = NULL;
}

/* accessor for rank to get a unique id for this process.
 * otherwise undocumented, may change; used in mtslib.c when
 * POSIX is defined for now
 */
int getrank(void)
{
	return rank;
}

int main(int argc, char **argv)
{
	mts_init(&argc, argv); /* mts commandline arguments etc */

	if (rank == 0)
		return mts_master(argc, argv);
	else if (rank == 1)
		return mts_consumer();
	else
		return mts_worker(argc, argv);
}

#define argint() (i+1<*argc ? atoi(argv[i+1]) : -1);
#define get_intarg(a, b, c) \
  if (!strcmp(argv[i], a)) { \
     arg = argint(); \
     if (arg < c) \
       bad_args(*argc,argv); \
     b = arg; \
     removearg(argc, argv, i); \
     removearg(argc, argv, i); \
     continue; \
  }
#define get_fn(a) \
  if (i+1>=*argc) \
	bad_args(*argc,argv); \
  a = argv[i+1]; \
  removearg(argc, argv, i); \
  removearg(argc, argv, i);

int mts_init(int *argc, char **argv)
{
	int i;
	long arg;
	void *v;
	int flag;
	/* start MPI */
	MPI_Init(argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_get_attr( MPI_COMM_WORLD, MPI_TAG_UB, &v, &flag );

	if (size<3)
	{
		printf("At least 3 processes required\n");
		MPI_Finalize();
		exit(0);
	}

	gettimeofday(&tstart, NULL);

	if (!flag)
		tagub = 32767; /* should be okay */
	else
		tagub = *(int*)v;

	while ((tagub-501)%size!=0) /* overflow on own tags if needed */
		tagub--; 

	for (i=1; i<*argc;)
	{
		get_intarg("-maxnodes", max_nodes, 1);
		get_intarg("-maxd", maxd, 0);
		get_intarg("-scale", scale, 1);
		get_intarg("-lmax", lmax, 1);
		get_intarg("-lmin", lmin, 1);
		get_intarg("-maxbuf", maxbuf, 1);
		if (!strcmp(argv[i], "-hist"))
		{
			get_fn(fn_hist);
			continue;
		}
		if (!strcmp(argv[i], "-freq"))
		{
			get_fn(fn_freq);
			continue;
		}
		if (!strcmp(argv[i], "-checkp"))
		{
			get_fn(fn_checkp);
			continue;
		}
		if (!strcmp(argv[i], "-stop"))
		{
			get_fn(fn_stop);
			continue;
		}
		if (!strcmp(argv[i], "-restart"))
		{
			get_fn(fn_restart);
			continue;
		}
		i++; /* leave unknown arguments for get_input() */
	}

	if (lmin==0)
		lmin = lmax;
	if (rank == 0 && (fn_hist != NULL || fn_freq != NULL))
		init_files();
	return 1;	
}

int mts_master(int argc, char **argv)
{
	Node *L = NULL;
	MPI_Request *incoming = (MPI_Request *)malloc(sizeof(MPI_Request)*2*size);
	MPI_Request *aborts = (MPI_Request *)malloc(sizeof(MPI_Request)*size);
	MPI_Request *in_sdata = (MPI_Request *)malloc(sizeof(MPI_Request)*size);
	unsigned long *unexp_buffer = (unsigned long *)malloc(sizeof(unsigned long)*6*size);
	int *abortbuf = (int *)malloc(sizeof(int)*size);
	long *sheaders = (long *)malloc(sizeof(long)*6*size);
	long *countbuf = (long *)malloc(sizeof(long)*size);
	int *firstwork = (int *)malloc(sizeof(int)*size);/* first work unit? */
						  /* used to omit 0th in freq*/
	mts_buff *inbuff = NULL;
	mts_buff *sdbuff = NULL;

	shared_data *sdata = (shared_data *)malloc(sizeof(shared_data)*size);
	long **gen = (long **)malloc(sizeof(long *)*size);

	int *busy = (int *)malloc(sizeof(int)*size); /* for histogram */
	struct timeval cur, last; /* for histogram */
	int size_L = 0;
	long tot_L = 0;
	int busy_workers = 0;
	int fin_sdworkers = 0; /* number of workers done with sdata */
	int i, j;
	long fin[5] = {-1};
	int start[2] = {1,0}; /* cleanstart, #sdata from restart */
	long nodecount=0, tmp;
	int loopinc = 0, flag = 0;
	int abort = 0;
	int index = 0;
	int docheckp = 1; /* do a checkpoint on unfinished exit */

	data.input = NULL;
	data.input_size = 0;

	gettimeofday(&last, NULL);

	if (fn_hist!=NULL && hist==NULL) /* histogram file error */
		start[0] = 0;
	if (fn_freq!=NULL && freq==NULL) /* frequency file error */
		start[0] = 0;

	for (i=0; i<size; i++)
	{
		gen[i] = (long *)malloc(sizeof(long)*size);
		for (j=0; j<size; j++)
			gen[i][j] = 0;

		sdata[i].sdlong = NULL;
		sdata[i].size_sdlong = 0;
		sdata[i].sdint = NULL;
		sdata[i].size_sdint = 0;
		sdata[i].sdchar = NULL;
		sdata[i].size_sdchar = 0;
		sdata[i].sdfloat = NULL;
		sdata[i].size_sdfloat = 0;
		sdata[i].modified = 0;
	}

	/* get data and root */
	if (start[0] != 0)
	{
		/* provided by application */
		start[0] = get_input(argc, argv, &data); 
		if (start[0] != 0)
			bdata = bts_init(argc, argv, &data, rank);

		if (fn_restart != NULL && bdata != NULL)
		{
			L = master_restart(&size_L, sdata, gen);
			tot_L = size_L;
			nodecount = 0;
		}
                else if (fn_restart == NULL && bdata != NULL)
		{
                        L = get_root(bdata);
			L->next = NULL;
			size_L = tot_L = nodecount = 1;
		}
		if (L == NULL || bdata == NULL) /*bdata==NULL implies L==NULL*/
			size_L = tot_L = start[0] = 0;
		flush_stream(streams); /*get_input may produce output for root*/
		flush_stream(streams+1);
	}

	/* check for shared_data updates from restart. */
	/* workers should have updates before work units */
	for (i=0; i<size; i++)
		if (gen[0][i]>gen[i][i])
			start[1]++;
			
	mts_debug(("M: cleanstart==%d #sdata=%d\n", start[0],start[1]));

	/* tell workers whether we got valid input and can start */
	for (i=2; i<size; i++)
		MPI_Send(start, 2, MPI_INT, i, 3, MPI_COMM_WORLD);

	if (start[0] == 0)
		goto master_done;

	for (i=0; i<size; i++)
	{
		busy[i] = 0;
		firstwork[i] = 1;
		if (i==0)
		{
			aborts[i] = MPI_REQUEST_NULL;
		}
		else
			MPI_Irecv(abortbuf+i, 1, MPI_INT, i, 10, MPI_COMM_WORLD,
				  aborts+i);
		if (i==0 || i==1)
		{
			incoming[i] = incoming[size+i] = MPI_REQUEST_NULL;
			in_sdata[i] = MPI_REQUEST_NULL;
			continue;
		}

		/* send input to each worker */
		send_data(i);
		/* send shared_data updates for restart */
		for (j=2; j<size; j++)
			if (gen[0][j]>gen[i][j])
			{
				gen[i][j] = gen[0][j];
				send_sdata(i,sdata+j, gen[0][j], j);
			}

		/* initialize receives for
		 * 1) request for work
		 * 2) returning unexplored nodes (nummber of nodes)
		 * 3) returning updated shared_data
		 */
		MPI_Recv_init(countbuf+i, 1, MPI_LONG, i, 1, MPI_COMM_WORLD,
			      incoming + size + i);
		MPI_Recv_init(unexp_buffer+6*i, 6, MPI_UNSIGNED_LONG, i, 2,
			      MPI_COMM_WORLD, incoming + i);
		MPI_Recv_init(sheaders+6*i, 6, MPI_LONG, i, 20, MPI_COMM_WORLD,
			      in_sdata + i);

		MPI_Start(incoming + size + i);
		MPI_Start(incoming + i);
		MPI_Start(in_sdata + i);
	}
#if 0 /* workers finish receive before asking for work, no need
       * for master to wait too 
       */
	while (outbuff!=NULL) /* wait for shared_data updates to finish */
		outbuff = mts_cleanbufs(outbuff);
#endif
	mts_debug(("M: entering main loop\n"));
	while ((size_L>0 && abort==0) || busy_workers>0)
	{

		if ((loopinc++)&0xff) /* sometimes do some things */
		{
			if (hist!=NULL)
				do_histogram(&cur, &last, size_L, busy,
					     busy_workers, tot_L);
			if ( (fn_stop!=NULL) && (abort==0) )
				abort = check_stop();
		}

		/* check for any aborts */
		if (abort == 0)
		{
		       MPI_Testany(size,aborts,&index,&abort,MPI_STATUS_IGNORE);
			if (abort)
			{
				mts_debug(("M: got abort from %d (cp %d)\n",
					   index, abortbuf[index]));
				docheckp = abortbuf[index];
			}
		}

		/* hand out work if possible */
		for (i=2; i<size; i++)
		{
			if (size_L<1 || abort!=0)
				break; /* no work to hand out now */
			MPI_Test(incoming+size+i, &flag, MPI_STATUS_IGNORE);
			if (!flag) /* i doesn't want work now */
				continue;
			L = send_node(i, L, size_L, &outbuff, sdata, gen);
			if (freq != NULL)
			{
				if (firstwork[i]!=1)
					fprintf(freq, "%ld\n", countbuf[i]);
				firstwork[i]=0;
			}
			MPI_Start(incoming + size + i);
			busy[i]++;
			busy_workers++;
			size_L--;
		}
		/* check for returned unexplored cobases */
		for (i=2; i<size; i++)
		{
			MPI_Test(incoming+i, &flag, MPI_STATUS_IGNORE);
			if (!flag)
				continue;
			inbuff = get_unexplored(i, unexp_buffer + 6*i, inbuff);
			MPI_Start(incoming + i);
			busy[i]--;
		}
		/* check for shared_data updates */
		for (i=2; i<size; i++)
		{
			MPI_Test(in_sdata+i, &flag, MPI_STATUS_IGNORE);
			if (!flag)
				continue;
			sdbuff = mts_addsdbuff(sheaders+6*i, sdbuff, i);
			MPI_Start(in_sdata + i);
		}

		outbuff = mts_cleanbufs(outbuff);
		inbuff = mts_checkunexp(inbuff, &L, &size_L, &tot_L,
					 &busy_workers);
		sdbuff = mts_cleansdbufs(sdbuff, sdata, gen);
	}

	/* all done */
	for (i=2; i<size; i++)
	{
		/* say goodbye to workers */
		mts_debug(("M: saying goodbye to %d\n", i));
		MPI_Wait(incoming+size+i, MPI_STATUS_IGNORE);
		if (freq != NULL)
		{
			if (firstwork[i]!=1)
				fprintf(freq, "%ld\n", countbuf[i]);
			firstwork[i] = 0;
		}
		MPI_Send(fin, 5, MPI_INT, i, 4, MPI_COMM_WORLD);
		MPI_Recv(&tmp, 1, MPI_LONG, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		nodecount += tmp;
		MPI_Request_free(incoming+size+i);
		MPI_Request_free(incoming+i);
	}

	/* make sure we get last shared_data updates */
	while (fin_sdworkers < size-2)
		for (i=2; i<size; i++)
		{
			MPI_Test(in_sdata+i, &flag, MPI_STATUS_IGNORE);
			if (!flag)
				continue;
			if (sheaders[6*i]<0)
			{
				fin_sdworkers++;
				mts_debug(("M: %d finished with sdata\n", i));
				continue;
			}
			sdbuff = mts_addsdbuff(sheaders+6*i, sdbuff, i);
			MPI_Start(in_sdata + i);
		}

master_done:
	/* finish any incoming shared_data updates */
	while (sdbuff!=NULL)
		sdbuff = mts_cleansdbufs(sdbuff, sdata, gen);
	
	if (start[0])
		bts_finish(bdata, sdata, size, docheckp);

	/* flush output to consumer */
	while (outbuff!=NULL)
		outbuff = mts_cleanbufs(outbuff);

	if (abort != 0 && docheckp != 0)
		master_checkpoint(L, sdata, size_L, gen);
	gettimeofday(&cur, NULL);
	fin[0] = nodecount;
	fin[1] = tot_L;
	fin[2] = cur.tv_sec - tstart.tv_sec;
	fin[3] = cur.tv_usec - tstart.tv_usec;
	if (fin[3] < 0) {
		--fin[2];
		fin[3] += 1000000;
	}
	MPI_Send(fin, 4, MPI_LONG, 1, 5, MPI_COMM_WORLD);

	MPI_Finalize();
	free(incoming);
	free(unexp_buffer);
	free(aborts);
	free(abortbuf);
	free(countbuf);
	free(sheaders);
	free(busy);
	free(firstwork);
	free(in_sdata);
	if (freq != NULL)
		fclose(freq);
	for (i=0; i<size; i++)
		free(gen[i]);
	free(gen);
	for (i=0; i<size; i++)
	{
		free(sdata[i].sdlong);
		free(sdata[i].sdint);
		free(sdata[i].sdchar);
		free(sdata[i].sdfloat);
	}
	free(sdata);
	return 0;
}

/* check if we want to stop now */
/* check if fn_stop exists */
int check_stop(void)
{
	FILE *a;
	a = fopen(fn_stop, "r");
	if (a==NULL)
		return 0;
	fclose(a);
	return 1;
}

/* fn_restart has a filename to restart from */
Node *master_restart(int *size_L, shared_data *sdata, long **gen)
{
	FILE *restart = fopen(fn_restart, "r");
	shared_data *sd = NULL;
	char c;
	int j,len;
	unsigned int i;
	int cdata, bad, csize_L, csize;
	Node *n, *head=NULL;
	if (restart == NULL)
	{
		fprintf(stderr,"mts: unable to restart from checkpoint file\n");
		return NULL;
	}
	len = strlen(name);
	i=fscanf(restart, "mts%d\n", &cdata);
	if (i!=1)
	{
		fprintf(stderr, "mts: invalid checkpoint file\n");
		return NULL;
	}
	if (cdata!=1)
	{
		fprintf(stderr, "mts: checkpoint file from unknown, newer version\n");
		return NULL;
	}
	for (i=0;i<len;i++)
	{
		c = fgetc(restart);
		if (c!=name[i])
		{
			fprintf(stderr, "mts: checkpoint file does not match application name\n");
			return NULL;
		}
	}
	c = fgetc(restart);
	if (c!='\n')
	{
		fprintf(stderr, "mts: checkpoint file does not match application name\n");
		return NULL;
	}

	fscanf(restart, "%u\n", &i);
	if (i != data.input_size)
		fprintf(stderr, "*mts: Warning, checkpoint file doesn't match input file, continuing anyway\n");
	bad = 0;
	for (j=0; j<i && j<data.input_size; j++)
		if ((cdata=fgetc(restart)) != data.input[j] &&
		    !(cdata=='\n' && data.input[j]=='\0' && j+1==i))
			bad = 1;
	if (bad && i==data.input_size)
		fprintf(stderr, "*mts: Warning, checkpoint file doesn't match input file, continuing anyway\n");
	while (cdata!='\n')
		cdata = fgetc(restart);

	fscanf(restart, "%d %d\n", &csize_L, &csize);
	if (csize != size)
		fprintf(stderr, "*mts: Warning, restarting with a different number of processes (originally %d).  shared_data may be inconsistent.\n", csize);

	for (j=0; j<csize_L; j++)
	{
		n = (Node *)malloc(sizeof(Node));
		n->next = head;
		fscanf(restart, "%u %u %u %u %d %ld ", &n->size_vlong,
		       &n->size_vint, &n->size_vchar, &n->size_vfloat,
		       &n->unexplored, &n->depth);
		n->vlong = NULL; n->vint = NULL;
		n->vchar = NULL; n->vfloat = NULL;
		if (n->size_vlong>0)
			n->vlong = (long *)malloc(sizeof(long)*n->size_vlong);
		if (n->size_vint>0)
			n->vint = (int *)malloc(sizeof(int)*n->size_vint);
		if (n->size_vchar>0)
			n->vchar = (char *)malloc(sizeof(char)*n->size_vchar);
		if (n->size_vfloat>0)
			n->vfloat = (float *)malloc(sizeof(float)*n->size_vfloat);
		for (i=0; i<n->size_vlong; i++)
			fscanf(restart, " %ld", n->vlong+i);
		for (i=0; i<n->size_vint; i++)
			fscanf(restart, " %d", n->vint+i);
		for (i=0; i<n->size_vchar; i++)
			fscanf(restart, " %c", n->vchar+i);
		for (i=0; i<n->size_vfloat; i++)
			fscanf(restart, " %f", n->vfloat+i);
		fgetc(restart); /* newline */
		head = n;
	}

	*size_L = csize_L;
	for (i=0; i<csize && i<size; i++)
	{
		sd = sdata+i;
		fscanf(restart, "%ld %u %u %u %u", &(gen[0][i]),
			&(sd->size_sdlong),&(sd->size_sdint),&(sd->size_sdchar),
			&(sd->size_sdfloat));
		if (gen[0][i]>0)
			gen[0][i] = 1; /* restarted gen >0 and <first
					* shared_data update
					*/
		if (sd->size_sdlong>0)
			sd->sdlong = (long *)malloc(sizeof(long)*sd->size_sdlong);
		if (sd->size_sdint>0)
			sd->sdint = (int *)malloc(sizeof(int)*sd->size_sdint);
		if (sd->size_sdchar>0)
			sd->sdchar = (char *)malloc(sizeof(char)*sd->size_sdchar);
		if (sd->size_sdfloat>0)
			sd->sdfloat = (float *)malloc(sizeof(float)*sd->size_sdfloat);
		for (j=0; j<sd->size_sdlong; j++)
			fscanf(restart, " %ld", sd->sdlong+j);
		for (j=0; j<sd->size_sdint; j++)
			fscanf(restart, " %d", sd->sdint+j);
		for (j=0; j<sd->size_sdchar; j++)
			fscanf(restart, " %c", sd->sdchar+j);
		for (j=0; j<sd->size_sdfloat; j++)
			fscanf(restart, " %f", sd->sdfloat+j);
		fgetc(restart); /* newline */
	}
	fclose(restart);
	return head;
}

void master_checkpoint(Node *L, shared_data *sdata, int size_L, long **gen)
{
	Node *n;
	FILE *checkp;
	long i;
	unsigned int j;
	if (fn_checkp != NULL)
	{
		checkp = fopen(fn_checkp, "w");
		if (checkp == NULL)
		{
			fprintf(stderr, "*mts: unable to write to checkpoint file. Checkpoint file follows this line on stderr\n");
			checkp = stderr;
		}
	}
	else
		checkp = stderr;
	/* header, application name and data */
	fprintf(checkp,"mts1\n%s\n%u\n%s\n", name, data.input_size, data.input);
	fprintf(checkp, "%d %d\n", size_L, size);
	for (n=L; n; n=n->next)
	{
		fprintf(checkp,"%u %u %u %u %d %ld ",n->size_vlong,n->size_vint,
					      n->size_vchar, n->size_vfloat,
					      n->unexplored, n->depth);
		for (i=0; i<n->size_vlong; i++)
			fprintf(checkp, " %ld", n->vlong[i]);
		for (i=0; i<n->size_vint; i++)
			fprintf(checkp, " %d", n->vint[i]);
		for (i=0; i<n->size_vchar; i++)
			fprintf(checkp, " %c", n->vchar[i]);
		for (i=0; i<n->size_vfloat; i++)
			fprintf(checkp, " %f", n->vfloat[i]);
		fprintf(checkp, "\n");
	}
	for (i=0; i<size; i++)
	{
		fprintf(checkp,"%ld %u %u %u %u",gen[0][i],sdata[i].size_sdlong,
					sdata[i].size_sdint, 
					sdata[i].size_sdchar,
					sdata[i].size_sdfloat);
		for (j=0; j<sdata[i].size_sdlong; j++)
			fprintf(checkp, " %ld", sdata[i].sdlong[j]);
		for (j=0; j<sdata[i].size_sdint; j++)
			fprintf(checkp, " %d", sdata[i].sdint[j]);
		for (j=0; j<sdata[i].size_sdchar; j++)
			fprintf(checkp, " %c", sdata[i].sdchar[j]);
		for (j=0; j<sdata[i].size_sdfloat; j++)
			fprintf(checkp, " %f", sdata[i].sdfloat[j]);
		fprintf(checkp, "\n");
	}
	/* if user specifies /dev/stderr we get the message only here */
	if (fn_checkp != NULL && checkp == stderr)
		fprintf(stderr, "*mts: checkpoint file finished above this line\n");
}

void send_data(int target)
{
	mts_debug(("M: sending data to %d\n",target));
	MPI_Send(&(data.input_size), 1, MPI_INT, target, 3, MPI_COMM_WORLD);
	MPI_Send(data.input, data.input_size, MPI_CHAR, target, 3, MPI_COMM_WORLD);
/*	MPI_Send(&(data.size_treeroot), 1, MPI_INT, target, 3, MPI_COMM_WORLD);
	MPI_Send(data.treeroot, data.size_treeroot, MPI_INT, target, 3, MPI_COMM_WORLD);
*/
	return;
}

void recv_data(Data *d)
{
	MPI_Recv(&(d->input_size), 1, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	d->input = (char *)malloc(sizeof(char)*d->input_size);
	MPI_Recv(d->input, d->input_size, MPI_CHAR, 0, 3, MPI_COMM_WORLD,
		 MPI_STATUS_IGNORE);
	mts_debug(("%d: got data\n",rank));
/*
  	MPI_Recv(&(d->size_treeroot), 1, MPI_INT, 0, 3, MPI_COMM_WORLD,
		MPI_STATUS_IGNORE);
	d->treeroot = malloc(sizeof(int)*d->size_treeroot);
	MPI_Recv(d->treeroot, d->size_treeroot, MPI_INT, 0, 3, MPI_COMM_WORLD,
		 MPI_STATUS_IGNORE);
*/
}

Node *send_node(int target, Node *L, int size_L, mts_buff **outbuff,
		shared_data *sdata, long **gen)
{
	long *buf = (long *)malloc(sizeof(long)*9);
	Node *next;
	MPI_Request *req = (MPI_Request *)malloc(5*sizeof(MPI_Request));
	mts_buff *newbuf = (mts_buff *)malloc(sizeof(mts_buff));
	int count=0, i;

	for (i=0; i<5; i++)
		req[i] = MPI_REQUEST_NULL;

	for (i=0; i<size; i++)
		if (gen[0][i]>gen[target][i])
			count++; /* want to update this shared_data */

	mts_debug(("M: sending work to %d\n", target));

	next = L->next;
	buf[0] = L->size_vlong;
	buf[1] = L->unexplored;
	buf[2] = L->depth;
	buf[3] = LONG_MAX;
	buf[4] = max_nodes;
	buf[5] = count;
	buf[6] = L->size_vint;
	buf[7] = L->size_vchar;
	buf[8] = L->size_vfloat;

	if (size_L > size * lmax) /* L too big */
		buf[4] *= scale;
	if (size_L < size * lmin) /* L too small */
		buf[3] = L->depth + maxd;
	if (buf[3]<L->depth || maxd==0) /* overflow or maxd disabled */
		buf[3] = LONG_MAX;

	MPI_Isend(buf, 9, MPI_LONG, target, 4, MPI_COMM_WORLD, req);
	if (L->size_vlong>0)
		MPI_Isend(L->vlong, L->size_vlong, MPI_LONG, target, 4, 
		  	  MPI_COMM_WORLD, req+1);
	if (L->size_vint>0)
		MPI_Isend(L->vint, L->size_vint, MPI_INT, target, 4,
			  MPI_COMM_WORLD, req+2);
	if (L->size_vchar>0)
		MPI_Isend(L->vchar, L->size_vchar, MPI_CHAR, target, 4,
			  MPI_COMM_WORLD, req+3);
	if (L->size_vfloat>0)
		MPI_Isend(L->vfloat, L->size_vfloat, MPI_FLOAT, target, 4,
			  MPI_COMM_WORLD, req+4);

	newbuf->req = req;
	newbuf->buf = (void **)malloc(5*sizeof(void *));
	newbuf->buf[0] = buf;
	newbuf->buf[1] = L->vlong;
	newbuf->buf[2] = L->vint;
	newbuf->buf[3] = L->vchar;
	newbuf->buf[4] = L->vfloat;
	newbuf->sizes = NULL;
	newbuf->types = NULL;
	newbuf->tags = NULL;
	newbuf->num = 5;
	newbuf->target = target;
	newbuf->queue = 0;
	newbuf->next = *outbuff;
	*outbuff = newbuf;

	for (i=0; i<size; i++)
                if (gen[0][i]>gen[target][i])
                {
			gen[target][i] = gen[0][i];  /* updating */
			send_sdata(target, sdata+i, gen[0][i], i);
		}

	free(L);
	return next;
}

mts_buff *mts_addsheader(mts_buff *sheaders)
{
	mts_buff *buf = (mts_buff *)malloc(sizeof(mts_buff));
	buf->req = (MPI_Request *)malloc(sizeof(MPI_Request));
	buf->buf = (void **)malloc(sizeof(void *));
	buf->sizes = (unsigned int *)malloc(sizeof(unsigned int));
	buf->types = NULL;
	buf->tags = NULL;
	buf->num = 1;
	buf->target = 0;
	buf->buf[0] = malloc(sizeof(long)*6);
	buf->sizes[0] = 6;
	MPI_Irecv(buf->buf[0], 6, MPI_LONG, 0, 20, MPI_COMM_WORLD, buf->req);
	buf->next = sheaders;
	return buf;
}

/* sheaders is the list of incoming shared_data headers.
 * if any have completed, queue the corresponding shared_data sends
 * in sdbuff.
 */
mts_buff *proc_sheaders(mts_buff *sheaders, mts_buff **sdbuff)
{
	mts_buff *tmp, *next, *prev=NULL, *ret = sheaders;
	int flag;

	for (tmp=sheaders; tmp; tmp=next)
	{
		next = tmp->next;
		MPI_Test(tmp->req, &flag, MPI_STATUS_IGNORE);
		if (!flag)
		{
			prev = tmp;
			continue;
		}
		*sdbuff = mts_addsdbuff((long *)(tmp->buf[0]), *sdbuff, 0);
		freembuff(tmp);
		if (prev != NULL)
			prev->next = next;
		else
			ret = next;
	}
	return ret;
}

/* size_v == -1 means finished */
mts_buff *recv_node(Node *node, long *max_depth, long *max_nodes, int target,
		    mts_buff *sheaders)
{
	long buf[9]={0};
	int sdnum; /* number of updated shared_data to receive */
	int i;

	MPI_Recv(buf, 9, MPI_LONG, target,4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	if (buf[0]==-1)
	{
		node->size_vlong = -1;
		node->vlong = NULL;
		return sheaders;
	}

	mts_debug(("%d: receiving node (%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld)\n",
		   rank,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],
		   buf[8]));
	if (buf[0]>0)
	{
		node->vlong = (long *)malloc(sizeof(long)*buf[0]);
		MPI_Recv(node->vlong, buf[0],MPI_LONG,target,4, MPI_COMM_WORLD, 
			 MPI_STATUS_IGNORE);
	}
	else
		node->vlong = NULL;
	if (buf[6]>0)
	{
		node->vint = (int *)malloc(sizeof(int)*buf[6]);
		MPI_Recv(node->vint, buf[6], MPI_INT, target, 4, MPI_COMM_WORLD,
			 MPI_STATUS_IGNORE);
	}
	else
		node->vint = NULL;

	if (buf[7]>0)
	{
		node->vchar = (char *)malloc(sizeof(char)*buf[7]);
		MPI_Recv(node->vchar, buf[7], MPI_CHAR, target,4,MPI_COMM_WORLD,
			 MPI_STATUS_IGNORE);
	}
	else
		node->vchar = NULL;
	
	if (buf[8]>0)
	{
		node->vfloat = (float *)malloc(sizeof(float)*buf[8]);
		MPI_Recv(node->vfloat, buf[8], MPI_FLOAT, target, 4,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	else
		node->vfloat = NULL;

	node->size_vlong = buf[0];
	node->unexplored = buf[1];
	node->depth = buf[2];
	node->next = NULL;
	if (max_depth!=NULL)
		*max_depth = buf[3];
	if (max_nodes!=NULL)
		*max_nodes = buf[4];

	sdnum = buf[5];
	node->size_vint = buf[6];
	node->size_vchar = buf[7];
	node->size_vfloat = buf[8];
 
	if (sdnum==0)
		return sheaders;

	/* queue incoming shared data updates */
	for (i=0; i<sdnum; i++)
		sheaders = mts_addsheader(sheaders);
	
	return sheaders;
}

/* return this list to the master */
void return_unexplored(Node *list)
{
	unsigned long count=0;
	unsigned long tot_sizel=0, tot_sizei=0, tot_sizec=0, tot_sizef=0;
	unsigned long *header = (unsigned long *)malloc(sizeof(unsigned long)*6);
	long  *nodel = NULL;
	int   *nodei = NULL;
	char  *nodec = NULL;
	float *nodef = NULL;
	unsigned int *sizes = NULL;
	long *other = NULL; /* depths and unexplored */
	int i;
	unsigned int ll, ii, cc, ff;
	MPI_Request *req = (MPI_Request *)malloc(sizeof(MPI_Request)*7);
	mts_buff *newbuff = (mts_buff *)malloc(sizeof(mts_buff));
	Node *tmp, *next;

	newbuff->req = req;
	newbuff->buf = (void **)malloc(sizeof(void *)*7);
	newbuff->sizes = NULL;
	newbuff->types = NULL;
	newbuff->tags = NULL;
	newbuff->queue = 0;

	for (i=0; i<7; i++)
		req[i] = MPI_REQUEST_NULL;

	for (tmp=list; tmp; tmp=tmp->next)
	{
		count++;
		tot_sizel += tmp->size_vlong;
		tot_sizei += tmp->size_vint;
		tot_sizec += tmp->size_vchar;
		tot_sizef += tmp->size_vfloat;
	}

	header[0] = count;
	header[1] = tot_sizel;
	header[2] = mytag;
	header[3] = tot_sizei;
	header[4] = tot_sizec;
	header[5] = tot_sizef;

	MPI_Isend(header, 6, MPI_UNSIGNED_LONG, 0, 2, MPI_COMM_WORLD, req);
	mts_debug(("%d: returning unexplored (%lu,%lu,%d,%lu,%lu,%ld)\n",rank,
		   count,tot_sizel,mytag,tot_sizei,tot_sizec,tot_sizef));

	if (tot_sizel>0)
		nodel = (long *)malloc(sizeof(long)*tot_sizel);
	if (tot_sizei>0)
		nodei = (int *)malloc(sizeof(int)*tot_sizei);
	if (tot_sizec>0)	
		nodec = (char *)malloc(sizeof(char)*tot_sizec);
	if (tot_sizef>0)
		nodef = (float *)malloc(sizeof(float)*tot_sizef);

	sizes = (unsigned int *)malloc(sizeof(unsigned int)*4*count);
	other = (long *)malloc(sizeof(long)*2*count);

	ll = 0;
	ii = 0;
	cc = 0;
	ff = 0;
	for (i=0,tmp=list; tmp; i++,tmp=next)
	{
		sizes[4*i] = tmp->size_vlong;
		sizes[4*i+1]= tmp->size_vint;
		sizes[4*i+2]= tmp->size_vchar;
		sizes[4*i+3]= tmp->size_vfloat;
		other[2*i] = tmp->depth;
		other[2*i+1] = tmp->unexplored;
		if (tot_sizel>0)
		       memcpy(nodel+ll,tmp->vlong,tmp->size_vlong*sizeof(long));
		ll += tmp->size_vlong;
		if (tot_sizei>0)
			memcpy(nodei+ii,tmp->vint,tmp->size_vint*sizeof(int));
		ii += tmp->size_vint;
		if (tot_sizec>0)
			memcpy(nodec+cc, tmp->vchar,
			       tmp->size_vchar*sizeof(char));
		cc += tmp->size_vchar;
		if (tot_sizef>0)
			memcpy(nodef+ff, tmp->vfloat,
			       tmp->size_vfloat*sizeof(float));
		ff += tmp->size_vfloat;

		next = tmp->next;
		free(tmp->vlong);
		free(tmp->vint);
		free(tmp->vchar);
		free(tmp->vfloat);
		free(tmp);
	}
	if (tot_sizel>0)
		MPI_Isend(nodel, tot_sizel, MPI_LONG, 0, mytag, 
			  MPI_COMM_WORLD, req+1);
	if (tot_sizei>0)
		MPI_Isend(nodei, tot_sizei, MPI_INT, 0, mytag,
			  MPI_COMM_WORLD, req+4);
	if (tot_sizec>0)
		MPI_Isend(nodec, tot_sizec, MPI_INT, 0, mytag,
			  MPI_COMM_WORLD, req+5);
	if (tot_sizef>0)
		MPI_Isend(nodef, tot_sizef, MPI_INT, 0, mytag,
			  MPI_COMM_WORLD, req+6);

	MPI_Isend(sizes, 4*count, MPI_INT, 0, mytag, MPI_COMM_WORLD, req+2);
	MPI_Isend(other, 2*count, MPI_LONG, 0, mytag, MPI_COMM_WORLD, req+3);

	newbuff->buf[0] = header;
	newbuff->buf[1] = nodel;
	newbuff->buf[2] = sizes;
	newbuff->buf[3] = other;
	newbuff->buf[4] = nodei;
	newbuff->buf[5] = nodec;
	newbuff->buf[6] = nodef;
	newbuff->num = 7;
	newbuff->target = 0;
	newbuff->next = outbuff;
	mytag++;
	if (mytag>499)
		mytag = 100;
	outbuff = newbuff;
}

mts_buff *get_unexplored(int target, unsigned long *header, mts_buff *inbuff)
{
	unsigned long num_nodes = header[0];
	unsigned long tot_sizel = header[1];
	unsigned long tag = header[2];
	unsigned long tot_sizei = header[3];
	unsigned long tot_sizec = header[4];
	unsigned long tot_sizef = header[5];
	int i;

	MPI_Request *req = (MPI_Request *)malloc(sizeof(MPI_Request)*6);
	mts_buff *newbuff = (mts_buff *)malloc(sizeof(mts_buff));
	long *nodel = NULL;
	int *nodei = NULL;
	char *nodec = NULL;
	float *nodef = NULL;
	unsigned int *sizes = (unsigned int *)malloc(sizeof(int)*6*num_nodes);
	int *other = (int *)malloc(sizeof(long)*2*num_nodes);

	for (i=0; i<6; i++)
		req[i] = MPI_REQUEST_NULL;

	newbuff->buf = (void **)malloc(sizeof(void *)*6);
	newbuff->sizes = (unsigned int *)malloc(sizeof(int)*6);
	newbuff->types = NULL;
	newbuff->tags = NULL;

	if (tot_sizel>0)
		nodel = (long *)malloc(sizeof(long)*tot_sizel);
	if (tot_sizei>0)
		nodei = (int *)malloc(sizeof(int)*tot_sizei);
	if (tot_sizec>0)
		nodec = (char *)malloc(sizeof(char)*tot_sizec);
	if (tot_sizef>0)
		nodef = (float *)malloc(sizeof(float)*tot_sizef);

	for (i=0; i<6; i++)
		req[i] = MPI_REQUEST_NULL;

	if (tot_sizel>0)
		MPI_Irecv(nodel, tot_sizel, MPI_LONG, target, tag,
			  MPI_COMM_WORLD, req);
	if (tot_sizei>0)
		MPI_Irecv(nodei, tot_sizei, MPI_INT, target, tag,
			  MPI_COMM_WORLD, req+3);
	if (tot_sizec>0)
		MPI_Irecv(nodec, tot_sizec, MPI_CHAR, target, tag,
			  MPI_COMM_WORLD, req+4);
	if (tot_sizef>0)
		MPI_Irecv(nodef, tot_sizef, MPI_CHAR, target, tag,
			  MPI_COMM_WORLD, req+5);

	MPI_Irecv(sizes, 4*num_nodes, MPI_INT, target,tag,MPI_COMM_WORLD,req+1);
	MPI_Irecv(other, 2*num_nodes, MPI_LONG,target,tag,MPI_COMM_WORLD,req+2);

	newbuff->req = req;
	newbuff->num = 6;
	newbuff->target = target;
	newbuff->queue = 0;
	newbuff->buf[0] = nodel;
	newbuff->buf[1] = sizes;
	newbuff->buf[2] = other;
	newbuff->buf[3] = nodei;
	newbuff->buf[4] = nodec;
	newbuff->buf[5] = nodef;
	newbuff->sizes[0] = tot_sizel;
	newbuff->sizes[1] = 4*num_nodes;
	newbuff->sizes[2] = 2*num_nodes;
	newbuff->sizes[3] = tot_sizei;
	newbuff->sizes[4] = tot_sizec;
	newbuff->sizes[5] = tot_sizef;
	newbuff->next = inbuff;
	return newbuff;
}

/* unexp_buff is a completed return of unexplored nodes, process them
 * and add to L. doesn't free anything. 
 */
Node *proc_unexplored(mts_buff *unexp, Node *L, int *size_L, long *tot_L)
{
	/*int tot_size = unexp->sizes[0];*/
	unsigned long num_nodes = unexp->sizes[1]>>2;
	long *nodel = (long *)unexp->buf[0];
	int *nodei = (int *)unexp->buf[3];
	char *nodec = (char *)unexp->buf[4];
	float *nodef = (float *)unexp->buf[5];
	unsigned int *sizes = (unsigned int *)unexp->buf[1];
	long *other = (long *)unexp->buf[2];
	unsigned int sizel, sizei, sizec, sizef;
	Node *tmp, *start = L;
	unsigned long i;
	unsigned int ll, ii, cc, ff;

	*size_L+=num_nodes;
	*tot_L+=num_nodes;

	ll = ii = cc = ff = 0;
	for (i=0; i<num_nodes; i++)
	{
		tmp = (Node *)malloc(sizeof(Node));
		sizel = sizes[4*i];
		sizei = sizes[4*i+1];
		sizec = sizes[4*i+2];
		sizef = sizes[4*i+3];

		if (sizel>0)
		{
			tmp->vlong = (long *)malloc(sizeof(long)*sizel);
			memcpy(tmp->vlong, nodel + ll, sizel*sizeof(long));
		}
		else
			tmp->vlong = NULL;
		ll += sizel;

		if (sizei>0)
		{
			tmp->vint = (int *)malloc(sizeof(int)*sizei);
			memcpy(tmp->vint, nodei + ii, sizei*sizeof(int));
		}
		else
			tmp->vint = NULL;
		ii += sizei;

		if (sizec>0)
		{
			tmp->vchar = (char *)malloc(sizeof(char)*sizec);
			memcpy(tmp->vchar, nodec + cc, sizec*sizeof(char));
		}
		else
			tmp->vchar = NULL;
		cc += sizec;

		if (sizef>0)
		{
			tmp->vfloat = (float *)malloc(sizeof(float)*sizef);
			memcpy(tmp->vfloat, nodef + ff, sizef*sizeof(float));
		}
		else
			tmp->vfloat = NULL;
		ff += sizef;

		tmp->size_vlong = sizel;
		tmp->size_vint = sizei;
		tmp->size_vchar = sizec;
		tmp->size_vfloat = sizef;
		tmp->depth = other[2*i];
		tmp->unexplored = other[2*i+1];
		tmp->next = start;
		start = tmp;
	}
	return start;
}

void freembuff(mts_buff *i)
{
	int j;
	free(i->req);
	for (j=0; j<i->num; j++)
		free(i->buf[j]);
	free(i->buf);
	free(i->sizes);
	free(i->types);
	free(i->tags);
	free(i);
}

mts_buff *mts_checkunexp(mts_buff *head, Node **L, int *size_L, 
			   long *tot_L, int *busy_workers)
{
	mts_buff *ret = head, *prev = NULL, *i, *next;
	int flag;

	for (i=head; i; i=next)
	{
		next = i->next;
		MPI_Testall(i->num, i->req, &flag, MPI_STATUSES_IGNORE);
		if (!flag) /* this one not finished yet */
		{
			prev = i;
			continue;
		}
		/* i is finished */
		*L = proc_unexplored(i, *L, size_L, tot_L);
		(*busy_workers)--;
		freembuff(i);
		if (prev != NULL)
			prev->next = next;
		else /* i == head */
			ret = next;
	}
	return ret;
}

/* check list of incoming shared_data transfers. If any have completed,
 * remove from list, and update sdata and gen.  Note that if sdata
 * already has a newer gen of a newly-completed shared_data update -
 * the newly-completed transfer is discarded without changing sdata/gen.
 */
/* Previously, gen could be NULL - meaning don't track generations.
 * keep that behavior for now, though it's not used.
 */
/* TODO? should combine with above, keep incoming buffers all in one place
 */
mts_buff *mts_cleansdbufs(mts_buff *head, shared_data *sdata, long **gen)
{
	mts_buff *ret = head, *prev = NULL, *i, *next;
	int flag;

	for (i=head; i; i=next)
	{
		next = i->next;
		MPI_Testall(i->num, i->req, &flag, MPI_STATUSES_IGNORE);
		if (!flag) /* this one not finished yet */
		{
			prev = i;
			continue;
		}
		/* i is finished */
		mts_procsdata(i, sdata, gen);
		freembuff(i);
		if (prev != NULL)
			prev->next = next;
		else /* i == head */
			ret = next;
	}
	return ret;
}

/* update sdata&gen with completed b. if the current sdata
 * from this target is newer than b, don't make changes.
 */
void mts_procsdata(mts_buff *b, shared_data *sdata, long **gen)
{
	int whose = b->aux2;

	int sizel=b->sizes[0], sizei=b->sizes[1], sizec=b->sizes[2], 
	    sizef=b->sizes[3];

	shared_data *sd = sdata+whose;
	if (gen!=NULL && b->aux <= gen[0][whose])
		return; /* already have newer from target */
 	/* applications may deallocate space, change types, etc */
	free(sd->sdlong);
	free(sd->sdint);
	free(sd->sdchar);
	free(sd->sdlong);
	sd->sdlong = (long *)b->buf[0];
	sd->sdint = (int *)b->buf[1];
	sd->sdchar = (char *)b->buf[2];
	sd->sdfloat = (float *)b->buf[3];
	sd->size_sdlong = sizel;
	sd->size_sdint = sizei;
	sd->size_sdchar = sizec;
	sd->size_sdfloat = sizef;
	b->buf[0] = NULL; /* we claimed these buffers    */
	b->buf[1] = NULL; /* not great... but for now... */
	b->buf[2] = NULL;
	b->buf[3] = NULL;
	sd->modified = 1;
#if 0
	buf[data_size] = '\0'; /* null-terminate for safe debug printing */
#endif
	if (gen!=NULL)
		gen[0][whose] = gen[whose][whose] = b->aux;
	mts_debug(("\n%d: got %d's shared data, gen %ld from %d\n", rank, whose,
		   b->aux, b->target));
}

mts_buff *mts_cleanbufs(mts_buff *head)
{
	mts_buff *ret = head, *prev=NULL, *i, *next = NULL;
	int flag, j;
	int queue;
	for (i=head; i; i=next)
	{
		next = i->next;
		queue = i->queue;
		if (queue!=1)
			MPI_Testall(i->num, i->req, &flag, MPI_STATUSES_IGNORE);
		else
			MPI_Test(i->req, &flag, MPI_STATUS_IGNORE);
		if (!flag) /* this one not finished yet */
		{
			prev = i;
			continue;
		}
		if (queue == 1) /* first part of queued send completed, */
		{		/* send remainder and move on to next buffer */
			for (j=1; j<i->num; j++)
				MPI_Isend(i->buf[j], i->sizes[j], i->types[j],
					  i->target, i->tags[j], MPI_COMM_WORLD,
					  i->req+j);
			i->queue = 0;
			prev = i;
			continue;
		}
		/* i is finished */
		freembuff(i);
		if (prev != NULL)
			prev->next = next;
		else /* i == head */
			ret = next;
	}
	return ret;
}

/* send this output now.  pointer output is relinquished to send_output,
 * will be freed later. dest is MTSOUT or MTSERR
 */
void send_output(char *output, int dest)
{
	int *len;
	mts_buff *buf;

	if (output == NULL)
		return;

	if (rank == 1)
	{
		if (dest == MTSOUT)
			printf("%s", output);
		else if (dest == MTSERR)
			fprintf(stderr, "%s", output);
		free(output);
		return;
	}

	len = (int *)malloc(sizeof(int)*3);
	buf = (mts_buff *)malloc(sizeof(mts_buff));
	buf->req = (MPI_Request *)malloc(sizeof(MPI_Request)*2);
	buf->buf = (void **)malloc(sizeof(void *)*2);
	buf->sizes = (unsigned int *)malloc(sizeof(unsigned int)*2);
	buf->types = (MPI_Datatype *)malloc(sizeof(MPI_Datatype)*2);
	buf->tags = (int *)malloc(sizeof(int)*2);
	buf->num = 2;
	buf->target = 1;
	buf->queue = 1;
	buf->next = outbuff;

	len[0] = strlen(output) + 1;
	len[1] = mytag;
	len[2] = dest;

	MPI_Isend(len, 3, MPI_INT, 1, 1, MPI_COMM_WORLD, buf->req);
	/*MPI_Isend(output, *len,MPI_CHAR, 1,1,MPI_COMM_WORLD,buf->req+1); */
	buf->buf[0] = len;
	buf->buf[1] = output;

	buf->sizes[0] = 3;
	buf->sizes[1] = len[0];
	buf->types[0] = MPI_INT;
	buf->types[1] = MPI_CHAR;
	buf->tags[0] = 2;
	buf->tags[1] = mytag;

	mytag++;
	outbuff = buf;
	return;
}

int mts_consumer(void)
{
	MPI_Request *incoming = (MPI_Request *)malloc(sizeof(MPI_Request)*size);
	int *inpsize = (int *)malloc(sizeof(int)*3*size);
	mts_buff *inbuff = NULL;
	long stats[4] = {0,0,0};
	int *workers = (int *)malloc(sizeof(int)*size);
	int i;
	int flag;
	int num_workers = size - 2;
	fprintf(stderr,"*mts:%s ", name);
	fprintf(stderr,TITLE);
	fprintf(stderr,VERSION);
	fprintf(stderr,"\n");
	fprintf(stderr,"*%d processes, maxd=%ld maxnodes=%ld scale=%d lmin=%d",
                size, maxd, max_nodes, scale, lmin);
	fprintf(stderr," lmax=%d maxbuf=%d\n", lmax, maxbuf);

	for (i=0; i<size; i++)
	{
		if (i==1)
		{
			incoming[i] = MPI_REQUEST_NULL;
			continue;
		}
		workers[i] = 1;
		MPI_Recv_init(inpsize+3*i, 3, MPI_INT, i, 1, MPI_COMM_WORLD,
			      incoming+i);
		MPI_Start(incoming+i);
	}

	while (num_workers>0 || inbuff!=NULL)
	{
		for (i=0; i<size; i++)
		{
			if (i==1)
				continue;
			MPI_Test(incoming+i, &flag, MPI_STATUS_IGNORE);
			if (!flag) /* no incoming output now */
				continue;
			if (i>1 && inpsize[3*i]<0)
			{
				if (workers[i]==0)
					continue;
				workers[i] = 0;
				num_workers--;
				mts_debug(("C: %d done, %d left\n",i,
					   num_workers));
				continue;
			}
			inbuff = mtscon_getincoming(inpsize+3*i, i, inbuff);
			MPI_Start(incoming+i);
		}
		inbuff = mtscon_checkincoming(inbuff);
	}

	mts_debug(("C: getting counting stats from master\n"));
	MPI_Recv(stats, 4, MPI_LONG, 0, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	fprintf(stderr,"Total nodes: %ld  Total number of jobs: %ld   Elapsed time: %0.6f seconds.\n\n",
		stats[0], stats[1], (stats[2] + stats[3] / 1000000.0));
	MPI_Finalize();
	free(incoming);
	free(workers);
	free(inpsize);
	return 0;
}

/* target sent us this header, queue the irecv in an mts_buff */
mts_buff *mtscon_getincoming(int *header, int target, mts_buff *inbuff)
{
	char *inp = (char *)malloc(sizeof(char)*header[0]);
	mts_buff *newbuf= (mts_buff *)malloc(sizeof(mts_buff));
	mts_debug(("C: queue incoming from %d\n",target));
	newbuf->req = (MPI_Request *)malloc(sizeof(MPI_Request));
	newbuf->buf = (void **)malloc(sizeof(void *));
	newbuf->sizes = NULL;
	newbuf->types = NULL;
	newbuf->tags = NULL;
	newbuf->num = 1;
	newbuf->queue = 0;
	newbuf->next = inbuff;
	newbuf->buf[0] = inp;
	newbuf->aux = header[2];
	MPI_Irecv(inp, header[0], MPI_CHAR, target, header[1], MPI_COMM_WORLD,
		  newbuf->req);
	return newbuf;
}

/* check our list of incoming mts_buffs to see if any have completed.
 * if so, printf() fflush() and free()
 */
mts_buff *mtscon_checkincoming(mts_buff *head)
{
	mts_buff *ret = head, *prev=NULL, *i, *next = NULL;
        int flag;
	int dest;
	for (i=head; i; i=next)
	{
		next = i->next;
		MPI_Testall(i->num, i->req, &flag, MPI_STATUSES_IGNORE);
		if (!flag)
		{
			prev=i;
			continue;
		}
		dest = i->aux; /* MTSOUT or MTSERR */
		if (dest == MTSOUT)
			printf("%s", (char *)i->buf[0]);
		else if (dest == MTSERR)
			fprintf(stderr, "%s", (char *)i->buf[0]);
		else
			printf("%d\n", dest);
		freembuff(i);
		if (prev != NULL)
			prev->next = next;
		else /* i == head */
			ret = next;
	}
	fflush(stdout);
	return ret;
}

void mtswor_cleanbufs(mts_buff **outbuff, mts_buff **sheaders,
		      mts_buff **sdbuff, shared_data *sdata, long **gen)
{
	*outbuff = mts_cleanbufs(*outbuff);
	*sheaders = proc_sheaders(*sheaders, sdbuff);
	*sdbuff = mts_cleansdbufs(*sdbuff, sdata, gen);
}

int mts_worker(int argc, char **argv)
{
	Node *node = (Node *)malloc(sizeof(Node));
	MPI_Request req;
	long max_depth, max_nodes;
	long done[6] = {-1};
	int idone[3] = {-1};
	long nodecount = 0; /* total count so far for this worker */
	long lnodes = 0; /* count in most recent job */
	int flag;
	int start[2] = {1,0};
	int i;
	shared_data *sdata;
	mts_buff *sheaders = NULL; /* incoming sdata update headers */
	mts_buff *sdbuff = NULL;   /* incoming sdata updates */
	long **gen = NULL; /* generation of my shared data */
			  /* ** to share code with master */
	data.input = NULL;	

	MPI_Recv(start, 2, MPI_INT, 0, 3, MPI_COMM_WORLD,
		 MPI_STATUS_IGNORE);

	mts_debug(("%d: cleanstart==%d #sdata=%d\n",rank,start[0],start[1]));

	sdata = (shared_data *)malloc(sizeof(shared_data) * size);
	gen = (long **)malloc(sizeof(long *) * size);
	gen[0] = (long *)malloc(sizeof(long)*size);

	for (i=0; i<size; i++)
	{
		sdata[i].sdlong = NULL;
		sdata[i].size_sdlong = 0;
		sdata[i].sdint = NULL;
		sdata[i].size_sdint = 0;
		sdata[i].sdchar = NULL;
		sdata[i].size_sdchar = 0;
		sdata[i].sdfloat = NULL;
		sdata[i].size_sdfloat = 0;
		sdata[i].modified = 0;
		gen[i] = gen[0]; /* only need single array */
		gen[0][i] = 0;   /* but share code with master */
	}

	if (start[0] == 0)
		goto worker_done;

	recv_data(&data); /* get input data */
	/* get shared_data updates on restart */
	for (i=0; i<start[1]; i++)
		sheaders = mts_addsheader(sheaders);
	while (sheaders!=NULL || sdbuff!=NULL)
		mtswor_cleanbufs(&outbuff, &sheaders, &sdbuff, sdata, gen);

	gen[0][rank] = 2; /* my initial generation */
	MPI_Send_init(&lnodes, 1, MPI_LONG, 0, 1, MPI_COMM_WORLD, &req);
	MPI_Start(&req);
	mts_debug(("%d: calling bts_init\n",rank));
	bdata = bts_init(argc, argv, &data, rank);
	mts_debug(("%d: bts_init returned\n",rank));

	while (1)
	{
		unexp = NULL;
		/* get work */
		flag = 0;
		while (1) /* was MPI_Wait(&req, MPI_STATUS_IGNORE); */
		{
			MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
			if (flag)
				break;
			mtswor_cleanbufs(&outbuff,&sheaders,&sdbuff,sdata,gen);
		}

		sheaders = recv_node(node, &max_depth, &max_nodes, 0,sheaders);
		
		if (node->size_vlong == -1)
			break;

		outblock = 0; /* close any open outputblock */

		/* do work, provided by application */
		mts_debug(("%d: calling bts\n",rank));
		lnodes = bts(bdata, node, max_depth, max_nodes, sdata,
			     rank, size);
		nodecount += lnodes;
		mts_debug(("%d: bts returned\n",rank));
		return_unexplored(unexp);
		MPI_Start(&req); /* request work */
		flush_stream(streams); flush_stream(streams+1); /*send output*/
		if (sdata[rank].modified)
		{
			mts_debug(("%d: modified sdata, sending\n",rank));
			send_sdata(0, sdata+rank, gen[0][rank]++, rank);
		}
		for (i=0; i<size; i++)
			sdata[i].modified = 0;
		free(node->vlong);
		free(node->vint);
		free(node->vchar);
		free(node->vfloat);
		mtswor_cleanbufs(&outbuff, &sheaders, &sdbuff, sdata, gen);
	}

	mts_debug(("%d: finished, cleaning buffers\n",rank));
	while (outbuff!=NULL || sheaders!=NULL || sdbuff!=NULL)
		mtswor_cleanbufs(&outbuff, &sheaders, &sdbuff, sdata, gen);
	mts_debug(("%d: clean buffers, informing master&consumer\n",rank));
	MPI_Send(&done, 6, MPI_LONG, 0, 20, MPI_COMM_WORLD); /* end of sdata */
	MPI_Send(&nodecount, 1, MPI_LONG, 0, 1, MPI_COMM_WORLD);
worker_done:
	MPI_Send(&idone, 3, MPI_INT, 1, 1, MPI_COMM_WORLD);
	free(node);
	freebdata(bdata);
	free(data.input);
	for (i=0; i<size; i++)
	{
		free(sdata[i].sdlong);
		free(sdata[i].sdint);
		free(sdata[i].sdchar);
		free(sdata[i].sdfloat);
	}
	free(sdata);
	free(gen[0]);
	free(gen);
/*	free(data.treeroot); */
	MPI_Finalize(); /* may exit here and not print next line */
/*      fprintf(stderr,"worker nodes=%ld \n",nodecount);*/
	return 0;
}

void return_unexp(const Node *node)
{
	Node *tmp = (Node *)malloc(sizeof(Node));
	unsigned int sizel = node->size_vlong;
	unsigned int sizei = node->size_vint;
	unsigned int sizec = node->size_vchar;
	unsigned int sizef = node->size_vfloat;

	if (sizel>0)
	{
		tmp->vlong = (long *)malloc(sizeof(long)*sizel);
		memcpy(tmp->vlong, node->vlong, sizeof(long)*sizel);
	}
	else
		tmp->vlong = NULL;
	if (sizei>0)
	{
		tmp->vint = (int *)malloc(sizeof(int)*sizei);
		memcpy(tmp->vint, node->vint, sizeof(int)*sizei);
	}
	else
		tmp->vint = NULL;
	if (sizec>0)
	{
		tmp->vchar = (char *)malloc(sizeof(char)*sizec);
		memcpy(tmp->vchar, node->vchar, sizeof(char)*sizec);
	}
	else
		tmp->vchar = NULL;
	if (sizef>0)
	{
		tmp->vfloat = (float *)malloc(sizeof(float)*sizef);
		memcpy(tmp->vfloat, node->vfloat, sizeof(float)*sizef);
	}
	else
		tmp->vfloat = NULL;

	tmp->size_vlong = sizel;
	tmp->size_vint = sizei;
	tmp->size_vchar = sizec;
	tmp->size_vfloat = sizef;
	tmp->unexplored = node->unexplored; /* should be true */
	tmp->depth = node->depth;
	tmp->next = unexp;
	unexp = tmp;
	return;
}

/* Bad commandline arguments. Complain and die. */
void bad_args(int argc, char **argv)
{
	int i, j;
	if (rank == 1)
	{
		printf("Invalid arguments.\nUsage: mpirun -np <num_proc> %s", 
		       argv[0]);
		for (i=0; i<mts_options_size; i++)
		{
			printf(" %s", mts_options[i].optname);
			if (mts_options[i].num_args == 1)
				printf(" <arg>");
			else
				for (j=0; j<mts_options[i].num_args; j++)
					printf(" <arg%d>", j);
		}
		for (i=0; i<bts_options_size; i++)
		{
			printf(" %s", bts_options[i].optname);
			if (bts_options[i].num_args == 1)
				printf(" <arg>");
			else
				for (j=0; j<bts_options[i].num_args; j++)
					printf(" <arg%d>", j);
		}
		printf("\n");
	}
	MPI_Finalize();
	exit(0);
}

/* remove argv[i] shifting everything later left one */
int removearg(int *argc, char **argv, int i)
{
	int j;
	for (j=i; j+1< *argc; j++)
		argv[j]=argv[j+1];
	(*argc)--;
	return 1;
}

/* open (fn_hist, fn_freq) for writing as (hist, freq) if not NULL */
void init_files(void)
{
	if (fn_hist != NULL)
	{
		hist = fopen(fn_hist, "w");
		if (hist == NULL)
			printf("Error opening histogram file\n");
	}
	if (fn_freq != NULL)
	{
		freq = fopen(fn_freq, "w");
		if (freq == NULL)
			printf("Error opening frequency file\n");
	}
}

/* check if we should update the histogram file, and do so */
void do_histogram(struct timeval *cur, struct timeval *last, int size_L,
		  int *busy, int busy_workers, long tot_L)
{
	float sec;
	long inc;
	long gran = 1000000; /* granularity of histogram in microseconds */
	int i, act;

	gettimeofday(cur,NULL);
	inc = (cur->tv_sec-last->tv_sec)*1000000 + (cur->tv_usec-last->tv_usec);

	if (inc > gran)
	{
		sec = (float)(cur->tv_sec - tstart.tv_sec) +
		      ((float)(cur->tv_usec - tstart.tv_usec))/1000000;
	/* time (seconds), number of active producers, number of
	 * producers owing us a message about remaining cobases,
	 * 0, 0 (existed in mplrs.cpp but no longer)
	 */
	act=0;
	for (i=0; i<size; i++)
		if (busy[i])
			act++;
	fprintf(hist, "%f %d %d %d %d %d %ld\n",
		sec, act, size_L, busy_workers, 0, 0, tot_L);
	fflush(hist);
	last->tv_sec = cur->tv_sec;
	last->tv_usec = cur->tv_usec;
	}
}

/* signal the master that we want to stop,
 * allows checkpoint.  Use your streams to
 * give output if desired.
 * cleanstop() returns control to the caller,
 * which must exit cleanly from bts().
 */
void cleanstop(int checkpoint)
{
	MPI_Send(&checkpoint, 1, MPI_INT, 0, 10, MPI_COMM_WORLD);
	return;
}

/* kills all processes, exits directly */
void emergencystop(char *msg)
{
	int ret;
	fprintf (stderr,"\n%s\n",msg);
	ret = MPI_Abort(MPI_COMM_WORLD, 1);
	exit(ret);
}

/* send *sdata to target, where this is generation gen */
/* data belongs to whose (!=rank if 0 is sending someone else's) */
void send_sdata(int target, shared_data *sdata, long gen, int whose)
{
	long *header = (long *)malloc(sizeof(long)*6);
	long *sdl = NULL;
	int *sdi = NULL;
	char *sdc = NULL;
	float *sdf = NULL;
	mts_buff *buf = (mts_buff *)malloc(sizeof(mts_buff));
	int i;

	buf->req = (MPI_Request *)malloc(sizeof(MPI_Request)*5);
	buf->buf = (void **)malloc(sizeof(void *)*5);
	buf->sizes = (unsigned int *)malloc(sizeof(int)*5);
	buf->types = (MPI_Datatype *)malloc(sizeof(MPI_Datatype)*5);
	buf->tags = (int *)malloc(sizeof(int)*5);

	for (i=0; i<5; i++)
		buf->req[i] = MPI_REQUEST_NULL;

	buf->buf[0] = header;
	header[0] = sdata->size_sdlong;
	header[1] = gen;
	header[2] = whose;
	header[3] = sdata->size_sdint;
	header[4] = sdata->size_sdchar;
	header[5] = sdata->size_sdfloat;

	if (sdata->size_sdlong>0)
	{
		sdl = (long *)malloc(sizeof(long)*sdata->size_sdlong);
		memcpy(sdl, sdata->sdlong, sdata->size_sdlong*sizeof(long));
	}
	if (sdata->size_sdint>0)
	{
		sdi = (int *)malloc(sizeof(int)*sdata->size_sdint);
		memcpy(sdi, sdata->sdint, sdata->size_sdint*sizeof(int));
	}
	if (sdata->size_sdchar>0)
	{
		sdc = (char *)malloc(sizeof(char)*sdata->size_sdchar);
		memcpy(sdc, sdata->sdchar, sdata->size_sdchar*sizeof(char));
	}
	if (sdata->size_sdfloat>0)
	{
		sdf = (float *)malloc(sizeof(float)*sdata->size_sdfloat);
		memcpy(sdf, sdata->sdfloat, sdata->size_sdfloat*sizeof(float));
	}

	buf->buf[1] = sdl;
	buf->buf[2] = sdi;
	buf->buf[3] = sdc;
	buf->buf[4] = sdf;

	buf->sizes[0] = 6;
	buf->sizes[1] = sdata->size_sdlong;
	buf->sizes[2] = sdata->size_sdint;
	buf->sizes[3] = sdata->size_sdchar;
	buf->sizes[4] = sdata->size_sdfloat;
	buf->types[0] = MPI_INT;
	buf->types[1] = MPI_LONG;
	buf->types[2] = MPI_INT;
	buf->types[3] = MPI_CHAR;
	buf->types[4] = MPI_FLOAT;
	buf->tags[0] = 20;
	buf->tags[1] = 500+(whose+size*gen)%(tagub-501);
	buf->tags[2] = 500+(whose+size*gen)%(tagub-501);
	buf->tags[3] = 500+(whose+size*gen)%(tagub-501);
	buf->tags[4] = 500+(whose+size*gen)%(tagub-501);
	buf->num = 5;
	buf->target = target;
	buf->queue = 0;
	buf->aux = 0;
	buf->next = outbuff;
	outbuff = buf;

	mts_debug(("%d: sending sdata (%ld,%ld,%ld,%ld,%ld,%ld)\n", rank, 
   		 header[0],header[1],header[2],header[3],header[4],header[5]));
	MPI_Isend(header, 6, MPI_LONG, target, 20, MPI_COMM_WORLD, buf->req);
#if 1
	for (i=1; i<5; i++)
		if (buf->sizes[i]>0)
		       MPI_Isend(buf->buf[i],buf->sizes[i],buf->types[i],target,
				 buf->tags[i], MPI_COMM_WORLD, buf->req+i);
	/* better to queue? */
#endif
	return;
}

/* target sent us this header for shared data. queue the receive and add it
 * to the sdbuff list
 */
mts_buff *mts_addsdbuff(long *header, mts_buff *sdbuff, int target)
{
	int sizel = header[0];
	long gen = header[1];
	int whose = header[2];
	int sizei = header[3];
	int sizec = header[4];
	int sizef = header[5];
	long *sdl = NULL;
	int *sdi = NULL;
	char *sdc = NULL;
	float *sdf = NULL;
	int i;

	mts_buff *s = (mts_buff *)malloc(sizeof(mts_buff));

	mts_debug(("%d: getting sdata from %d (%d,%ld,%d,%d,%d)\n", 
		   rank, target, sizel,gen,sizei,sizec,sizef));
	s->req = (MPI_Request *)malloc(sizeof(MPI_Request)*4);
	for (i=0; i<4; i++)
		s->req[i] = MPI_REQUEST_NULL;

	if (sizel>0)
		sdl = (long *)malloc(sizeof(long)*sizel);
	if (sizei>0)
		sdi = (int *)malloc(sizeof(int)*sizei);
	if (sizec>0)
		sdc = (char *)malloc(sizeof(char)*sizec);
	if (sizef>0)
		sdf = (float *)malloc(sizeof(char)*sizef);
	s->buf = (void **)malloc(sizeof(void *)*4);
	s->sizes = (unsigned int *)malloc(sizeof(unsigned int)*4);
	s->types = (MPI_Datatype *)malloc(sizeof(MPI_Datatype)*4);
	s->tags = (int *)malloc(sizeof(int)*4);
	s->num = 4;
	s->target = target;
	s->queue = 0;
	s->aux = gen;
	s->aux2 = whose;
	s->buf[0] = sdl;
	s->buf[1] = sdi;
	s->buf[2] = sdc;
	s->buf[3] = sdf;
	s->sizes[0] = sizel;
	s->sizes[1] = sizei;
	s->sizes[2] = sizec;
	s->sizes[3] = sizef;
	s->types[0] = MPI_LONG;
	s->types[1] = MPI_INT;
	s->types[2] = MPI_CHAR;
	s->types[3] = MPI_FLOAT;
	for (i=0; i<4; i++)
		s->tags[i] = 500+(whose+size*gen)%(tagub-501);

	for (i=0; i<4; i++)
		if (s->sizes[i]>0)
			MPI_Irecv(s->buf[i], s->sizes[i], s->types[i], target,
				  s->tags[i], MPI_COMM_WORLD, s->req+i);
	s->next = sdbuff;
	return s;
}
