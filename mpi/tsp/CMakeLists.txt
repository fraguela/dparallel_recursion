cmake_minimum_required( VERSION 2.6 )

add_executable( tsp_mpi main.cpp tsp.cpp args.cpp build.cpp tsp.h )

add_executable( tsp_mpi_sc main.cpp tsp.cpp args.cpp build.cpp tsp.h )
target_compile_definitions( tsp_mpi_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( tsp_mpi_sc tbbmalloc )

install( TARGETS tsp_mpi tsp_mpi_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
