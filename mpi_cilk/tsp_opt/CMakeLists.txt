cmake_minimum_required( VERSION 2.6 )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

add_executable( tsp_opt_mpicilk main.cpp tsp.cpp args.cpp build.cpp tsp.h )

add_executable( tsp_opt_mpicilk_sc main.cpp tsp.cpp args.cpp build.cpp tsp.h )
target_compile_definitions( tsp_opt_mpicilk_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( tsp_opt_mpicilk_sc tbbmalloc )

install( TARGETS tsp_opt_mpicilk tsp_opt_mpicilk_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
