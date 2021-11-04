cmake_minimum_required( VERSION 2.8.11 )


add_executable( uts_seq uts_seq.cpp uts.c ./rng/brg_sha1.c )
target_compile_definitions( uts_seq PUBLIC -DBRG_RNG )

install( TARGETS uts_seq
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
