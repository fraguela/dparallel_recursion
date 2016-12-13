cmake_minimum_required( VERSION 2.6 )

add_executable( treeadd_mpi node.cpp args.cpp par-alloc.cpp tree.h )
add_executable( treeadd_mpi_sc node.cpp args.cpp par-alloc.cpp tree.h )
target_compile_definitions( treeadd_mpi_sc PUBLIC -DSCALABLE_ALLOCATION )
target_link_libraries( treeadd_mpi_sc tbbmalloc )

install( TARGETS treeadd_mpi treeadd_mpi_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
