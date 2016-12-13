#include "wtime.h"
#include <sys/time.h>

void wtime(double *t)
{
  static double sec = -1.0;
  struct timeval tv;
  double dtmp;

  gettimeofday(&tv, 0);
  dtmp = (double)tv.tv_sec + 1.0e-6*tv.tv_usec;
  if (sec < 0.0) sec = dtmp;
  *t = dtmp - sec;
}

    
