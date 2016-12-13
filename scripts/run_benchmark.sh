#!/bin/bash
#
#$ -cwd
#$ -j y
#

if [ $# -lt 1 ]; then
	echo 'run_benchmark.sh <benchmark> [nprocs] [threads_per_process] [tasks_per_thread] [problem_size]'
	echo '  where <benchmark> is cg, fib, NQueens, quicksort, strassen, treeadd or tsp'
	echo '   The script will run in turn the sequential version,' 
	echo '  the dparallel_recursion version, the MPI-only version,'
	echo '  and, if available, the MPI+OpenMP version.'
	echo '   Since the MPI-only version has no threads, in this case it runs'
	echo '  nprocs*threads_per_process processes'
	echo
	echo '  Problem size definition:'
	echo '	  cg        -> NAS Parallel Benchmark class'
	echo '	  fib       -> fibonacci number to compute'
	echo '	  NQueens   -> number of rows (or colums) of the board'
	echo '	  quicksort -> number of elements to sort'
	echo '	  strassen  -> matrix size'
	echo '	  treeadd   -> number of levels of the binary tree'
	echo '	  tsp	    -> number of cities'
	exit -1
fi

bindir=`dirname $0`/../bin

bench=$1

if [ $# -lt 2 ]; then
	np=1
	echo Defaulting to $np 'process(es)'
else
	np=$2
fi

if [ $# -lt 3 ]; then
	n=4
	echo Defaulting to $n threads per process	
else
	n=$3
fi

if [ $# -lt 4 ]; then
	tpt=1
	echo Defaulting to $tpt tasks per thread	
else
	tpt=$4
fi


function runbenchmarkb () {
	execf=${bindir}/${1}
	#--map-by ppr:N:node <-> -npernode N
	mpirun -np $np --bind-to none -x OMP_NUM_THREADS=$n $execf $2 $tpt
}

function runbenchmarkmpi () {
	execf=${bindir}/${1}
   	let NPROCS=$n*$np
   	#Notice that actually mpi processes do not use sub-tasks, so $tpt is ignored
	mpirun -np $NPROCS --bind-to none $execf $2 $tpt
}

#module load openmpi/gcc/ib/1.8.1 boost/gcc/openmpi-1.7.2/1.55.0 intel/64/tbb/4.1_up3
#source ${I_TBB_HOME}/bin/tbbvars.sh intel64

ulimit -c 0

if [ $# -lt 5 ]; then
	case "$bench" in
		cg*)
		arg=B
		;;

		fib*)     
		arg=46
		;;

		NQueens*) 
		arg=15
		;;

		quicksort*)
		arg=100000000
		;;

		strassen*)
		arg="8192 256"
		;;
		
		treeadd*)
		arg="25 100"
		;;

		tsp*)
		arg="8388608 0"
		;;

		*) 
		echo Unknown benchmark $bench
		exit 1

	esac
else
	arg=$5
	case "$bench" in
		cg*)
		;;

		fib*)     
		;;

		NQueens*) 
		;;

		quicksort*)
		;;

		strassen*)
		arg="$arg 256"
		;;
		
		treeadd*)
		arg="$arg 100"
		;;

		tsp*)
		arg="$arg 0"
		;;

		*) 
		echo Unknown benchmark $bench
		exit 1

	esac	
fi

echo "Running '$bench $arg' on $np processes with $n threads/process and $tpt tasks per thread"

for b in ${bench}_seq ${bench} ${bench}_mpi ${bench}_mpiomp; do
	
	echo ------------------------------
	
	if [ -e ${bindir}/${b}_sc ]; then
		b=${b}_sc
	elif [ ! -e ${bindir}/${b} ]; then
		echo File ${bindir}/${b} not found
		exit -1
	fi

	if [[ ${b} =~ _seq$ || ${b} =~ _seq_ ]]; then
		echo Running sequential version
		${bindir}/${b} $arg
	else
		if [[ ${b} =~ _mpi$ || ${b} =~ _mpi_ ]]; then
			echo Running MPI-only version
			runbenchmarkmpi $b "$arg"
		else
			if [[ ${b} =~ _mpiomp$ || ${b} =~ _mpiomp_ ]]; then
				echo Running MPI+OpenMP version
			else
				echo Running dparallel_recursion version
			fi
			runbenchmarkb   $b "$arg"
		fi
	fi
done
