cmake_minimum_required( VERSION 2.6 )

add_executable( tsp main.cpp tsp.cpp args.cpp build.cpp tsp.h ${dprHeaders} )

add_executable( tsp_sc main.cpp tsp.cpp args.cpp build.cpp tsp.h ${dprHeaders} )
target_compile_definitions( tsp_sc PUBLIC -DSCALABLE_ALLOCATION )

install( TARGETS tsp tsp_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )

