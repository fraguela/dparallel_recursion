cmake_minimum_required( VERSION 2.6 )

add_executable( treeadd_opt_mpicilk node.cpp args.cpp par-alloc.cpp tree.h )

add_executable( treeadd_opt_mpicilk_sc node.cpp args.cpp par-alloc.cpp tree.h )
target_compile_definitions( treeadd_opt_mpicilk_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( treeadd_opt_mpicilk_sc tbbmalloc )

install( TARGETS treeadd_opt_mpicilk treeadd_opt_mpicilk_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
