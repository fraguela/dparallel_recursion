cmake_minimum_required( VERSION 2.8.11 )

add_executable( uts uts_par_rec.cpp uts.c ./rng/brg_sha1.c ${dprHeaders} )
target_compile_definitions( uts PUBLIC -DBRG_RNG -DDPARALLEL_RECURSION )

add_executable( uts_stack uts_par_rec.cpp uts.c ./rng/brg_sha1.c ${dprHeaders} )
target_compile_definitions( uts_stack PUBLIC -DBRG_RNG -DPARALLEL_STACK_RECURSION )

add_executable( uts_dstack uts_par_rec.cpp uts.c ./rng/brg_sha1.c ${dprHeaders} )
target_compile_definitions( uts_dstack PUBLIC -DBRG_RNG -DDPARALLEL_STACK_RECURSION )

install( TARGETS uts uts_stack uts_dstack
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )

