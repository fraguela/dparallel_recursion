/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

/* Original code from the Application Kernel Matrix by Cray */

#include <iostream>
#include <cstring>
#include <sys/time.h>
#include <algorithm>
#include <string>

#define ROWS 64
#define COLS 64
#define DMAX 64

int solution = -1;

typedef int  coor[2];
typedef char ibrd[ROWS][COLS];

struct cell {
  int   n;
  coor *alt;
  int   top;
  int   bot;
  int   lhs;
  int   rhs;
  int   left;
  int   above;
  int   next;
};

struct cell * gcells;

int  MIN_AREA;
ibrd BEST_BOARD;
coor MIN_FOOTPRINT;

int N;

/* compute all possible locations for nw corner for cell */
static int starts(int id, int shape, coor *NWS, struct cell *cells) {
  int i, n, top, bot, lhs, rhs;
  int rows, cols, left, above;

/* size of cell */
  rows  = cells[id].alt[shape][0];
  cols  = cells[id].alt[shape][1];

/* the cells to the left and above */
  left  = cells[id].left;
  above = cells[id].above;

/* if there is a vertical and horizontal dependence */
  if ((left >= 0) && (above >= 0)) {

     top = cells[above].bot + 1;
     lhs = cells[left].rhs + 1;
     bot = top + rows;
     rhs = lhs + cols;

/* if footprint of cell touches the cells to the left and above */
     if ((top <= cells[left].bot) && (bot >= cells[left].top) &&
         (lhs <= cells[above].rhs) && (rhs >= cells[above].lhs)) {
    	 n = 1; NWS[0][0] = top; NWS[0][1] = lhs;
     } else {
    	 n = 0;
     }

/* if there is only a horizontal dependence */
   } else if (left >= 0) {

/* highest initial row is top of cell to the left - rows */ 
     top = std::max(cells[left].top - rows + 1, 0);
/* lowest initial row is bottom of cell to the left */
     bot = std::min(cells[left].bot, ROWS);
     n   = bot - top + 1;

     for (i = 0; i < n; i++) {
         NWS[i][0] = i + top;
         NWS[i][1] = cells[left].rhs + 1;
     }

  } else {

/* leftmost initial col is lhs of cell above - cols */
     lhs = std::max(cells[above].lhs - cols + 1, 0);
/* rightmost initial col is rhs of cell above */
     rhs = std::min(cells[above].rhs, COLS);
     n   = rhs - lhs + 1;

     for (i = 0; i < n; i++) {
         NWS[i][0] = cells[above].bot + 1;
         NWS[i][1] = i + lhs;
     }
  }

  return (n);
}



/* lay the cell down on the board in the rectangular space defined
   by the cells top, bottom, left, and right edges. If the cell can
   not be layed down, return 0; else 1.
*/
static bool lay_down(int id, ibrd board, struct cell *cells) {
  int  i, j, top, bot, lhs, rhs;

  top = cells[id].top;
  bot = cells[id].bot;
  lhs = cells[id].lhs;
  rhs = cells[id].rhs;

  for (i = top; i <= bot; i++) {
	  for (j = lhs; j <= rhs; j++) {
		  if (board[i][j] == 0) {
			  board[i][j] = (char)id;
		  } else {
			  return false;
		  }
	  }
  }

  return true;
}


#define read_integer(file,var) \
  if ( fscanf(file, "%d", &var) == EOF ) { exit(-1); }

static void read_inputs(FILE * inputFile) {
  int i, j, n;

  read_integer(inputFile,n);
  N = n;
  
  gcells = (struct cell *) malloc((n + 1) * sizeof(struct cell));

  gcells[0].n     =  0;
  gcells[0].alt   =  0;
  gcells[0].top   =  0;
  gcells[0].bot   =  0;
  gcells[0].lhs   = -1;
  gcells[0].rhs   = -1;
  gcells[0].left  =  0;
  gcells[0].above =  0;
  gcells[0].next  =  0;

  for (i = 1; i < n + 1; i++) {

      read_integer(inputFile, gcells[i].n);
      gcells[i].alt = (coor *) malloc(gcells[i].n * sizeof(coor));

      for (j = 0; j < gcells[i].n; j++) {
          read_integer(inputFile, gcells[i].alt[j][0]);
          read_integer(inputFile, gcells[i].alt[j][1]);
      }

      read_integer(inputFile, gcells[i].left);
      read_integer(inputFile, gcells[i].above);
      read_integer(inputFile, gcells[i].next);
      }

  if (!feof(inputFile)) {
      read_integer(inputFile, solution);
  }
}

static void write_outputs() {
  int i, j;

    std::cout <<  "Minimum area = " << MIN_AREA << std::endl << std::endl;

    for (i = 0; i < MIN_FOOTPRINT[0]; i++) {
      for (j = 0; j < MIN_FOOTPRINT[1]; j++) {
          if (BEST_BOARD[i][j] == 0) {std::cout << " ";}
          else                       std::cout << static_cast<char>('A' + BEST_BOARD[i][j] - 1);
      }
      std::cout << std::endl;
    }
}

static void add_cell (int id, coor FOOTPRINT, ibrd BOARD, struct cell *CELLS)  {
	/* for each possible shape */
	for (int i = 0; i < CELLS[id].n; i++) {
		/* compute all possible locations for nw corner */
		ibrd board;
		coor footprint, NWS[DMAX];
		int nn = starts(id, i, NWS, CELLS);
		/* for all possible locations */
		for (int j = 0; j < nn; j++) {
			int area;
			cell *cells = CELLS;

			/* extent of shape */
			cells[id].top = NWS[j][0];
			cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
			cells[id].lhs = NWS[j][1];
			cells[id].rhs = cells[id].lhs + cells[id].alt[i][1] - 1;

			std::memcpy(board, BOARD, sizeof(ibrd));

			/* if the cell cannot be layed down, prune search */
			if (lay_down(id, board, cells)) {
				/* calculate new footprint of board and area of footprint */
				footprint[0] = std::max(FOOTPRINT[0], cells[id].bot+1);
				footprint[1] = std::max(FOOTPRINT[1], cells[id].rhs+1);
				area         = footprint[0] * footprint[1];

				/* if last cell */
				if (cells[id].next == 0) {
					/* if area is minimum, update global values */
					if (area < MIN_AREA) {
						MIN_AREA         = area;
						MIN_FOOTPRINT[0] = footprint[0];
						MIN_FOOTPRINT[1] = footprint[1];
						std::memcpy(BEST_BOARD, board, sizeof(ibrd));
						//bots_debug("N  %d\n", MIN_AREA);
					}

				/* if area is less than best area */
				} else if (area < MIN_AREA) {
					add_cell(cells[id].next, footprint, board,cells);
					/* if area is greater than or equal to best area, prune search */
				}
			}
		}
	}
}

int main(int argc, char** argv) {
	std::string filename;
	struct timeval t0, t1, t;

	if (argc > 1) {
		filename = std::string(argv[1]);
	}

	ibrd board;

	FILE * inputFile;
	inputFile = fopen(filename.c_str(), "r");
    
    if(NULL == inputFile) {
        std::cout << "Couldn't open " << filename << " for reading" << std::endl;
        exit(1);
    }
    
    /* read input file and initialize global minimum area */
    read_inputs(inputFile);
    MIN_AREA = ROWS * COLS;
    
    /* initialize board is empty */
    for (int i = 0; i < ROWS; i++)
    for (int j = 0; j < COLS; j++) board[i][j] = 0;

    coor footprint;
    /* footprint of initial board is zero */
    footprint[0] = 0;
    footprint[1] = 0;

    gettimeofday(&t0, NULL);
    add_cell(1, footprint, board, gcells);
    gettimeofday(&t1, NULL);
    timersub(&t1, &t0, &t);
    
    std::cout << "compute time: " <<( t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
    std::cout << "floorplan(" << filename << "): " << MIN_AREA << std::endl;
    write_outputs();
    
    return 0;
}
