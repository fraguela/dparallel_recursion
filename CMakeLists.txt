# dparallel_recursion: distributed parallel_recursion skeleton
# Copyright (C) 2015-2018 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# \file     CMakeLists.txt
# \brief    Main CMake Listsfile of the project
# \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
# \author   Basilio B. Fraguela <basilio.fraguela@udc.es>

cmake_minimum_required( VERSION 2.8.12.2 )

project( dparallel_recursion )

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(Red         "${Esc}[31m")
  set(Blue        "${Esc}[34m")
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel"
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)


option( ONLY_THREADS "Only use shared memory parallelism" OFF )
#OFF by default because of the super-slow validation of NQueens
option( VALIDATE     "Perform in-test validation"         OFF )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

if(NOT VALIDATE)
	add_definitions( -DNO_VALIDATE )
endif(NOT VALIDATE)

if( OPENBLAS_DIR )
  add_definitions( -DOPENBLAS )
  include_directories ( ${OPENBLAS_DIR}/include )
  link_libraries( ${OPENBLAS_DIR}/lib/libopenblas.a pthread )
endif( OPENBLAS_DIR )

install(DIRECTORY DESTINATION ${PROJECT_SOURCE_DIR}/bin)

if(ONLY_THREADS)
	#We still look for boost to speedup the Strassen's test matrix multiply
	#in case we ever compile tests/benchmarks for only shared memory
	find_package( Boost QUIET )
	if(Boost_FOUND)
		include_directories ( ${Boost_INCLUDE_DIRS} )
	endif(Boost_FOUND)
else(ONLY_THREADS)
	find_package( Boost REQUIRED )
	include_directories ( ${Boost_INCLUDE_DIRS} )
endif(ONLY_THREADS)

#add_custom_target(bin ALL 
#		  ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/bin
#		  BYPRODUCTS ${PROJECT_SOURCE_DIR}/bin)


if(NOT ONLY_THREADS)

	add_subdirectory( sequential )

	find_package( MPI )

	if ( MPI_CXX_FOUND )
		set( CMAKE_CXX_COMPILER ${MPI_CXX_COMPILER} )
		add_compile_options( ${MPI_CXX_COMPILE_FLAGS} )
		include_directories( ${MPI_CXX_INCLUDE_PATH} )
		set( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${MPI_CXX_LINK_FLAGS}" )
		link_libraries( ${MPI_CXX_LIBRARIES} )
	else ( MPI_CXX_FOUND )
		message(STATUS "==============================================================")
		message(STATUS )
		message(STATUS "No MPI compiler found. Perhaps you want to set one explicitly?")
		message(STATUS "To override the default compiler, set the environment variable")
		message(STATUS "     export CXX=/full/path/to/mpic++")
		message(STATUS "in bash or ksh or sh.  In csh or tcsh, use:")
		message(STATUS "     setenv CXX /full/path/to/mpic++")
		message(STATUS "before running the cmake command.")
		message(STATUS )
		message(STATUS "${Red}You can also set ONLY_THREADS to ON for non-MPI installation${ColourReset}")
		message(STATUS )
		message(STATUS "==============================================================")
	endif( MPI_CXX_FOUND )

	add_subdirectory( mpi )

	# Only supported from cmake 3.0
	#message(STATUS "${Blue}mpi_cilk directory is skipped by default.\n\
	#   Edit its CMakeLists.txt to adapt it to the Cilk Plus compilation chain \n\
	#   in your system and uncomment the #add_subdirectory( mpi_cilk ) line \n\
	#   in the main CMakeLists.txt if you want to compile these codes. ${ColourReset}")

	message(STATUS "${Blue}mpi_cilk directory is skipped by default.")
	message(STATUS "Edit its CMakeLists.txt to adapt it to the Cilk Plus compilation chain")
	message(STATUS "in your system and uncomment the #add_subdirectory( mpi_cilk ) line")
	message(STATUS "in the main CMakeLists.txt if you want to compile these codes. ${ColourReset}")
	#add_subdirectory( mpi_cilk )

	find_package( OpenMP QUIET )
	if( OPENMP_FOUND )
		add_subdirectory( mpi_openmp )
	else( OPENMP_FOUND )
		message(STATUS "====================================================================")
		message(STATUS )
		message(STATUS "No OpenMP compiler found. This is not needed for dparallel_recursion")
		message(STATUS "   so we simply skip the MPI+OpenMP tests in directory mpi_openmp")
		message(STATUS )
		message(STATUS "====================================================================")
	endif( OPENMP_FOUND )

endif(NOT ONLY_THREADS)

include_directories ( ${PROJECT_SOURCE_DIR}/include )
link_libraries( tbb tbbmalloc  )

if(NOT ONLY_THREADS)
	link_directories( ${Boost_LIBRARY_DIRS} )
	link_libraries( boost_serialization )
endif(NOT ONLY_THREADS)

# The kernel dprHeaders do not include Range.h DRange.h AliasVector.h and FillableAliasVector.h
 
set( dprHeaders 
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/dparallel_recursion.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/parallel_recursion.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/general_reference_wrapper.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/dpr_utils.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/dpr_mpi_comm.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/dpr_mpi_gather_scatter.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/EmptyBody.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/Arity.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/DInfo.h
	${PROJECT_SOURCE_DIR}/include/dparallel_recursion/BufferedDInfo.h )

#out are: AliasVector.h FillableAliasVector.h Range.h DRange.h

add_subdirectory( tests )

if(NOT ONLY_THREADS)
	add_subdirectory( benchmarks )
endif(NOT ONLY_THREADS)

install( DIRECTORY include/dparallel_recursion
         DESTINATION include
         FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" )

