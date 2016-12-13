cmake_minimum_required( VERSION 2.8.12.2 )

set( benchmarks fib NQueens quicksort strassen barnes_hut_loc )

#Not needed because this directory is only compiled if OpenMP is found in the upper CMakeLists.txt
#find_package( OpenMP REQUIRED )

add_compile_options( ${OpenMP_CXX_FLAGS} )
set( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}" )

foreach(benchmark ${benchmarks})
  add_executable( ${benchmark}_mpiomp ${benchmark}.cpp )
  install( TARGETS ${benchmark}_mpiomp
           RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
endforeach(benchmark)

add_subdirectory( treeadd )
add_subdirectory( tsp )
add_subdirectory( EP )
