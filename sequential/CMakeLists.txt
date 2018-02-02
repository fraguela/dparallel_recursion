cmake_minimum_required( VERSION 2.6 )

set( benchmarks fib NQueens quicksort strassen barnes_hut_loc )

foreach(benchmark ${benchmarks})
  add_executable( ${benchmark}_seq ${benchmark}.cpp )
  install( TARGETS ${benchmark}_seq
           RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
endforeach(benchmark)

if(NOT Boost_FOUND)
	target_compile_definitions( strassen_seq PRIVATE -DNOBOOST )
endif(NOT Boost_FOUND)

add_subdirectory( treeadd )
add_subdirectory( tsp )
add_subdirectory( EP )
