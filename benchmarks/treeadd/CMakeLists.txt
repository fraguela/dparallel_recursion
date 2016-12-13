cmake_minimum_required( VERSION 2.6 )

add_executable( treeadd node.cpp args.cpp tree.h ${dprHeaders} )

add_executable( treeadd_sc node.cpp args.cpp tree.h ${dprHeaders} )
target_compile_definitions( treeadd_sc PUBLIC -DSCALABLE_ALLOCATION )

install( TARGETS treeadd treeadd_sc
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
