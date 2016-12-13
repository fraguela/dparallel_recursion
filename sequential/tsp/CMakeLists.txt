cmake_minimum_required( VERSION 2.6 )


add_executable( tsp_seq main.c tsp.c args.c build.c tsp.h )
target_link_libraries( tsp_seq m )

add_executable( tsp_seq_sc main.c tsp.c args.c build.c tsp.h )
target_compile_definitions( tsp_seq_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( tsp_seq_sc m tbbmalloc )

install( TARGETS tsp_seq tsp_seq_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
