cmake_minimum_required( VERSION 2.8.12.2 )

if( ${UNIX} AND (NOT (${APPLE})) )
  link_libraries( rt )
endif( ${UNIX} AND (NOT (${APPLE})) )

set( benchmarks fib NQueens quicksort strassen barnes_hut_loc topsorts fib_stack NQueens_stack quicksort_stack barnes_hut_loc_stack floorplan_stack knapsack_stack topsorts_stack fib_dstack NQueens_dstack quicksort_dstack quicksort_dstack_gatherinput topsorts_dstack )

foreach(benchmark ${benchmarks})
  add_executable( ${benchmark} ${benchmark}.cpp ${dprHeaders} )
endforeach(benchmark)
target_compile_options( fib_dstack PRIVATE
	"--param=large-unit-insns=2000"
	"--param=inline-unit-growth=60" )
target_compile_definitions( floorplan_stack PUBLIC -DDPR_FORCE_ORDER_L_TO_R )
target_compile_definitions( knapsack_stack PUBLIC -DDPR_FORCE_ORDER_L_TO_R )

add_subdirectory( treeadd )
add_subdirectory( tsp )
add_subdirectory( EP )
add_subdirectory( uts )

install( TARGETS ${benchmarks}
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
