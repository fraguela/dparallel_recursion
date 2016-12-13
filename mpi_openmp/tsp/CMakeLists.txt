cmake_minimum_required( VERSION 2.6 )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )

add_executable( tsp_mpiomp main.cpp tsp.cpp args.cpp build.cpp tsp.h )

add_executable( tsp_mpiomp_sc main.cpp tsp.cpp args.cpp build.cpp tsp.h )
target_compile_definitions( tsp_mpiomp_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( tsp_mpiomp_sc tbbmalloc )

install( TARGETS tsp_mpiomp tsp_mpiomp_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
