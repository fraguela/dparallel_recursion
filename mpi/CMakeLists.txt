cmake_minimum_required( VERSION 2.6 )

set( benchmarks fib NQueens quicksort strassen barnes_hut_loc )

foreach(benchmark ${benchmarks})
  add_executable( ${benchmark}_mpi ${benchmark}.cpp )
  install( TARGETS ${benchmark}_mpi
           RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
endforeach(benchmark)

add_subdirectory( treeadd )
add_subdirectory( tsp )
add_subdirectory( EP )
add_subdirectory( uts )
add_subdirectory( topsorts )
