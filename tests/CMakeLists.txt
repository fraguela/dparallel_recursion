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
# \brief    CMake Listsfile for tests
# \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
# \author   Basilio B. Fraguela <basilio.fraguela@udc.es>

cmake_minimum_required( VERSION 2.8.12.2 )

if(${UNIX})
  if(${APPLE})
    #The reason is that in OSX El Capitan (10.11) we need cmake -E env
    cmake_minimum_required( VERSION 3.1.3 )
    set( CMAKE_MACOSX_RPATH ON )
  else(${APPLE})
    link_libraries( rt )
  endif(${APPLE})
endif(${UNIX})

add_definitions( -DPR_PART=partitioner::automatic )
add_compile_options( "-g" )

if(ONLY_THREADS)
	set( tests test_local )
else(ONLY_THREADS)
	set( tests test_distrAddVector test_distrAddVectorGen test_replicate test_gatherVector test_refs test_fib test_nqueens test_treeadd test_quicksort test_rvalue_correctness test_PrioritizeDM test_ranges test_local test_global test_distr test_collectives test_user_distributed_post )
endif(ONLY_THREADS)

foreach(test ${tests})
	add_executable( ${test} ${test}.cpp ${dprHeaders} )
endforeach(test)

## run tests

add_custom_target(tests
	DEPENDS ${tests}
COMMENT "Run tests")

if(ONLY_THREADS)

	foreach(test ${tests})
		add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo running ${test} : -------------)
	    if(${APPLE})                   
	      add_custom_command(TARGET tests PRE_BUILD 
	                        COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$ENV{LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH} ${test} || exit 1)
	    else(${APPLE})
	      add_custom_command(TARGET tests PRE_BUILD COMMAND ${test} || exit 1)
	    endif(${APPLE})
	    add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo ------------------------)
	endforeach(test)
	add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo ${tests} SHARED MEMORY TESTS DONE 
	COMMAND ${CMAKE_COMMAND} -E echo **SUCCESSFUL**)

else(ONLY_THREADS)

	#in some MPIs it is enough to run "COMMAND ${test} || exit 1"
	#tests with -np 1
	foreach(test ${tests})
		add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo 1p test ${test} : -------------)
		if(${APPLE})
			add_custom_command(TARGET tests PRE_BUILD  COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$ENV{LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH} ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 ${MPIEXEC_PREFLAGS} ${test} || exit 1)
		else(${APPLE})
			add_custom_command(TARGET tests PRE_BUILD  COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 ${MPIEXEC_PREFLAGS} ${test} || exit 1)
		endif(${APPLE})
		add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo ------------------------)
	endforeach(test)
	add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo ${tests} SINGLE PROCESS TESTS DONE 
	COMMAND ${CMAKE_COMMAND} -E echo **SUCCESSFUL**)


	#tests with -np 2
	foreach(test ${tests})
		add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo 1p test ${test} : -------------)
		if(${APPLE})
			add_custom_command(TARGET tests PRE_BUILD  COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$ENV{LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH} ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 2 ${MPIEXEC_PREFLAGS} ${test} || exit 1)
		else(${APPLE})
			add_custom_command(TARGET tests PRE_BUILD  COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 2 ${MPIEXEC_PREFLAGS} ${test} || exit 1)
		endif(${APPLE})
		add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo ------------------------)
	endforeach(test)
	add_custom_command(TARGET tests PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo ${tests} MULTIPROCESS TESTS DONE
	COMMAND ${CMAKE_COMMAND} -E echo **SUCCESSFUL**)

endif(ONLY_THREADS)

install( TARGETS ${tests}
		 RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin )
