cmake_minimum_required( VERSION 2.8.11 )

add_executable( uts_mpi_ws uts_dm.c stats.c dequeue.c dlist.c mpi_workstealing.c rng/brg_sha1.c uts.c )
target_compile_definitions( uts_mpi_ws PUBLIC -DBRG_RNG -D__MPI__ )
target_link_libraries( uts_mpi_ws m )

add_executable( uts_mpi_wm uts_dm.c stats.c dequeue.c dlist.c mpi_worksharing.c rng/brg_sha1.c uts.c )
target_compile_definitions( uts_mpi_wm PUBLIC -DBRG_RNG -D__MPI__ )
target_link_libraries( uts_mpi_wm m )

add_executable( uts_mpi_wm_nb uts_dm.c stats.c dequeue.c dlist.c mpi_worksharing.c rng/brg_sha1.c uts.c )
target_compile_definitions( uts_mpi_wm_nb PUBLIC -DBRG_RNG -DNONBLOCK -D__MPI__ )
target_link_libraries( uts_mpi_wm_nb m )

install( TARGETS uts_mpi_ws uts_mpi_wm uts_mpi_wm_nb
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )

