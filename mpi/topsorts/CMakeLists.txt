cmake_minimum_required( VERSION 2.8.11 )

add_executable( topsorts_mpi_mts mts.c mtslib.c btopsorts.c )
target_compile_definitions( topsorts_mpi_mts PUBLIC -DMTS )

install( TARGETS topsorts_mpi_mts
         RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )

