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
/// \file     NQueens.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>

using namespace std;

class Board {
public:

  static int N, Nsols;
  static const int MaxBoard = 19;

  static void setN(int n) {
    if ( n > MaxBoard ) {
      cerr << "Board too large\n";
      exit(EXIT_FAILURE);
    }
    N = n;
  }

  static void play(int boardsize) {
    setN(boardsize);
    Nsols = 0;
    Board b;
    b.resolve(0);
    cout << "Number of solutions: " << Nsols << endl;
  }

  Board()
  {
  }

  void resolve(int curRow) {
    for (int i = 0; i < N; i++) {
      int j;
      for (j = 0; j < curRow; j++)
        if ( (board_[j] == i) || ((board_[j] + (curRow-j)) == i) || ((board_[j] - (curRow-j)) == i) )
          break;
      if (j == curRow) {      //column j is a legal placement for a queen in row curRow
        board_[curRow] = i;
        if (curRow == (N-1)) { //We got a solution
          Nsols++;
        } else {
          resolve(curRow+1);
        }
      }
    }
  }

private:
  char board_[MaxBoard];
};

int Board::N, Board::Nsols;

int main(int argc, char **argv)
{ struct timeval t0, t1, t;

  int problem_sz = (argc == 1) ? 15 : atoi(argv[1]);
  gettimeofday(&t0, NULL);
  Board::play(problem_sz);
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);

}
