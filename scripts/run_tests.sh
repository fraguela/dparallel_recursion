#!/bin/bash
#
#$ -cwd
#$ -j y
#

bindir=`dirname $0`/../bin

tests='test_distrAddVector test_distrAddVectorGen test_replicate test_gatherVector test_refs test_fib test_nqueens test_treeadd test_quicksort test_rvalue_correctness test_PrioritizeDM test_ranges test_local test_global test_distr test_collectives'

for test in $tests; do
	mpirun -np 1 ${bindir}/${test} || exit 1
done

echo SINGLE PROCESS TESTS DONE 
echo '**SUCCESSFUL**'

for test in $tests; do
	mpirun -np 2 ${bindir}/${test} || exit 1
done

echo MULTIPROCESS TESTS DONE 
echo '**SUCCESSFUL**'
