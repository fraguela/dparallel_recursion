cmake_minimum_required( VERSION 2.6 )

add_executable( treeadd_mpiomp node.cpp args.cpp par-alloc.cpp tree.h )

add_executable( treeadd_mpiomp_sc node.cpp args.cpp par-alloc.cpp tree.h )
target_compile_definitions( treeadd_mpiomp_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( treeadd_mpiomp_sc tbbmalloc )

install( TARGETS treeadd_mpiomp treeadd_mpiomp_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
