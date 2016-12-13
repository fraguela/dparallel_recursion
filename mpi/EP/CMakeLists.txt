cmake_minimum_required( VERSION 2.6 )

add_compile_options( -Wno-write-strings )
include_directories( ../../sequential/EP ../../sequential/EP/common )

set( ep_common_files ../../sequential/EP/common/c_print_results.c ../../sequential/EP/common/c_randi8.c ../../sequential/EP/common/c_timers.c ../../sequential/EP/common/wtime.c ../../sequential/EP/common/npb-C.h ../../sequential/EP/npbparams.h )

add_executable( ep_mpi ep.cpp ${ep_common_files} )

install( TARGETS ep_mpi
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
