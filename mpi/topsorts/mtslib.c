/* routines used by both mts and user programs */

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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "mts.h"
#ifdef POSIX
extern int getrank(void);
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#define TRUE 1
#define FALSE 0

/* get_input and related string utilities     */
/* these should not require user modification */


int get_input(int argc, char **argv, Data *in)
{
  FILE *f = stdin;
  char *instr = NULL;
  int i,j;
  int flag;
/* simplistic handling of argv: first non parameter is input file name  */
/* more generally the user needs to supply valid options somehow        */

  for (i=1; i<argc; )
  {
    flag = 0;
    for (j=0; j<bts_options_size; j++)
    {
	if (strcmp(argv[i], bts_options[j].optname)==0)
	{
	   i+=1+bts_options[j].num_args;
	   flag = 1;
	   break;
	}
   }
   if (flag==0)
      break; /* found first non-option, non-parameter */
   }

  if (i < argc)
      { 
        fprintf(stderr,"*input file: %s\n",argv[i]);
        f = fopen (argv[i], "r");
      }

  if ( f!=NULL)
  {
    instr = mts_filetostr(f);
    fclose(f);
  }
  else
    instr = "\0";

  in->input_size = strlen(instr) + 1; /* for \0 */
  in->input = instr;
  return 1;
}

#if defined(POSIX) && defined(MTS) /* POSIX shm */
FILE *open_string(const char *string)
{
	FILE *f;
	char *nam;
	int slen, len;
	int fildes;
	int rank = getrank();

	slen = snprintf(NULL, 0, "/mts_%d", rank);
	nam = malloc(sizeof(char)*(slen+1));
	sprintf(nam, "/mts_%d", rank);
	fildes = shm_open(nam, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	free(nam);

	len = strlen(string);
	ftruncate(fildes, len+1);
	write(fildes, string, len+1);
	f = fdopen(fildes, "r");
	rewind(f);
	return f;
}
void close_string(void)
{
	int rank = getrank();
	int slen = snprintf(NULL, 0, "/mts_%d", rank);
	char *nam = malloc(sizeof(char)*(slen+1));
	sprintf(nam, "/mts_%d", rank);
	shm_unlink(nam);
	free(nam);
}
#else /* ANSI C89 -- also for OSX */
FILE *open_string(const char *string)
{
	FILE *f = tmpfile();
	fprintf(f, "%s", string);
	fflush(f);
	rewind(f);
	return f;
}
void close_string(void)
{
	return;
}
#endif



char *mts_filetostr(FILE *f)
{
        char *buf = (char *)malloc(sizeof(char)*256), *newbuf;
        unsigned int size = 256;
        unsigned int count = 0;
        int c='a';

        while (c!='\0' && c!=EOF)
        {
                c = fgetc(f);
                if (c==EOF)
                        c='\0';
                if (count>=size)
                {
                        newbuf = (char *)realloc(buf, sizeof(char)*(size<<1));
                        if (newbuf == NULL) /* uh oh, truncated */
                        {
                                free(buf);
                                return NULL;
                        }
                        size = (size<<1);
                        buf = newbuf;
                }
                buf[count++] = c;
        }
        return buf;
}
