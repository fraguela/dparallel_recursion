cmake_minimum_required( VERSION 2.6 )

if( ${UNIX} AND (NOT (${APPLE})) )
  link_libraries( rt )
endif( ${UNIX} AND (NOT (${APPLE})) )

set( benchmarks fib NQueens quicksort strassen barnes_hut_loc )

foreach(benchmark ${benchmarks})
  add_executable( ${benchmark} ${benchmark}.cpp ${dprHeaders} )
endforeach(benchmark)


add_subdirectory( treeadd )
add_subdirectory( tsp )
add_subdirectory( EP )

install( TARGETS ${benchmarks}
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
