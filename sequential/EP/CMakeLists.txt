cmake_minimum_required( VERSION 2.6 )

add_compile_options( -Wno-write-strings )
include_directories( common )

add_executable( ep_seq ep.cpp common/c_print_results.c common/c_randi8.c common/c_timers.c common/wtime.c common/npb-C.h npbparams.h )

install( TARGETS ep_seq
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
