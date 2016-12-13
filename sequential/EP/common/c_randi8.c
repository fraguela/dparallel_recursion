/*
  RAND functions using 64b INTEGERs

  F. CANTONNET - HPCL - GWU
*/

/*c---------------------------------------------------------------------
c---------------------------------------------------------------------*/

double randlc (double *x, double a)
{
  /*c---------------------------------------------------------------------
    c---------------------------------------------------------------------*/

/*c---------------------------------------------------------------------
c
c   This routine returns a uniform pseudorandom double precision number in the
c   range (0, 1) by using the linear congruential generator
c
c   x_{k+1} = a x_k  (mod 2^46)
c
c   where 0 < x_k < 2^46 and 0 < a < 2^46.  This scheme generates 2^44 numbers
c   before repeating.  The argument A is the same as 'a' in the above formula,
c   and X is the same as x_0.  A and X must be odd double precision integers
c   in the range (1, 2^46).  The returned value RANDLC is normalized to be
c   between 0 and 1, i.e. RANDLC = 2^(-46) * x_1.  X is updated to contain
c   the new seed x_1, so that subsequent calls to RANDLC using the same
c   arguments will generate a continuous sequence.
c
c   This routine should produce the same results on any computer with at least
c   48 mantissa bits in double precision floating point data.  On 64 bit
c   systems, double precision should be disabled.
c
c   David H. Bailey     October 26, 1990
c
c---------------------------------------------------------------------*/

  unsigned long long i246m1, Lx, La;
  double d2m46;

  d2m46 = 0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5;
  //d2m46 = pow( 0.5, 46 );

  i246m1 = 0x00003FFFFFFFFFFF;

  Lx = *x;
  La = a;

  Lx = (Lx*La)&i246m1;
  *x = (double) Lx;
  return (d2m46 * (*x));
}

/*c---------------------------------------------------------------------
c---------------------------------------------------------------------*/

void vranlc (int n, double *x_seed, double a, double y[]) {

/*c---------------------------------------------------------------------
c---------------------------------------------------------------------*/

/*c---------------------------------------------------------------------
c
c   This routine generates N uniform pseudorandom double precision numbers in
c   the range (0, 1) by using the linear congruential generator
c
c   x_{k+1} = a x_k  (mod 2^46)
c
c   where 0 < x_k < 2^46 and 0 < a < 2^46.  This scheme generates 2^44 numbers
c   before repeating.  The argument A is the same as 'a' in the above formula,
c   and X is the same as x_0.  A and X must be odd double precision integers
c   in the range (1, 2^46).  The N results are placed in Y and are normalized
c   to be between 0 and 1.  X is updated to contain the new seed, so that
c   subsequent calls to VRANLC using the same arguments will generate a
c   continuous sequence.  If N is zero, only initialization is performed, and
c   the variables X, A and Y are ignored.
c
c   This routine is the standard version designed for scalar or RISC systems.
c   However, it should produce the same results on any single processor
c   computer with at least 48 mantissa bits in double precision floating point
c   data.  On 64 bit systems, double precision should be disabled.
c
c---------------------------------------------------------------------*/

  int i;
  double x;
  unsigned long long i246m1, Lx, La;
  double d2m46;

  d2m46 = 0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*0.5*
    0.5*0.5*0.5*0.5*0.5*0.5;
  //  d2m46 = pow( 0.5, 46.0 );
  i246m1 = 0x00003FFFFFFFFFFF;

  x = *x_seed;
  Lx = x;
  La = a;
  
  for (i = 1; i <= n; i++)
    {
      Lx = ((Lx*La)&i246m1);
      x = (double) Lx;
      y[i] = d2m46 * x;
    }
  *x_seed = x;
}
