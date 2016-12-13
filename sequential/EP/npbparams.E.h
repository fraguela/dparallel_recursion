/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
 This file is part of dparallel_recursion.
 
 dparallel_recursion is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software  Foundation; either version 3, or (at your option) any later version.
 
 dparallel_recursion is distributed in the  hope that  it will  be  useful, 
 but  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 more details.
 
 You should have received a copy of  the GNU General  Public License along with
 dparallel_recursion. If not, see <http://www.gnu.org/licenses/>.
*/

///
/// \file     npbparams.E.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* CLASS = E */
/*
c  This file is generated automatically by the setparams utility.
c  It sets the number of processors and the class of the NPB
c  in this directory. Do not modify it by hand.
*/
#define	CLASS	 'E'
int 	M	 = 40;
#define	CONVERTDOUBLE	FALSE
#define COMPILETIME "13 Feb 2015"
#define NPBVERSION "2.3"
#define CS1 "icpc"
#define CS2 "icpc"
#define CS3 "(none)"
#define CS4 "-I../common"
#define CS5 "-mpicxx icpc -O3 -fpermissive -w"
#define CS6 "-mpicxx icpc -O3"
#define CS7 "randdp"
