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
/// \file     wtime.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* C/Fortran interface is different on different machines. 
 * You may need to tweak this.
 */


#if defined(IBM)
#define wtime wtime
#elif defined(CRAY)
#define wtime WTIME
#else
#define wtime wtime_
#endif
