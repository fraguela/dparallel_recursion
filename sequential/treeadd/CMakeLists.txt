cmake_minimum_required( VERSION 2.6 )

add_executable( treeadd_seq node.c args.c par-alloc.c tree.h )
add_executable( treeadd_seq_sc node.c args.c par-alloc.c tree.h )
target_compile_definitions( treeadd_seq_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( treeadd_seq_sc tbbmalloc )

install( TARGETS treeadd_seq treeadd_seq_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
