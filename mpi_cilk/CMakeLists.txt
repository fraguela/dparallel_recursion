cmake_minimum_required( VERSION 2.8.12.2 )

#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcilkplus" )
#
#if( ${APPLE} )
#  message(STATUS " Cilk codes compiled with ${CMAKE_CURRENT_SOURCE_DIR}/apple_mpicxx.sh")
#  set( CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/apple_mpicxx.sh )
#endif( ${APPLE} )
#
#link_libraries( cilkrts )

set( benchmarks fib fib_opt NQueens NQueens_opt quicksort quicksort_opt barnes_hut_loc strassen strassen_opt)

foreach(benchmark ${benchmarks})
  add_executable( ${benchmark}_mpicilk ${benchmark}.cpp )
 
 install( TARGETS ${benchmark}_mpicilk
           RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
endforeach(benchmark)

add_subdirectory( treeadd )
add_subdirectory( treeadd_opt )
add_subdirectory( tsp )
add_subdirectory( tsp_opt )
add_subdirectory( EP )
