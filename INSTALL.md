## Installation</p>


### Requirements


 * A C++ compiler that supports C++11 (g++ 5.2.1, g++ 6.1, and Apple LLVM 7.0.2 have been tested)
 * [CMake](https://cmake.org) 2.8.12.2 or above (3.1.3 for Mac OS X)
 * [Intel® Threading Building Blocks (TBB)](http://threadingbuildingblocks.org) v3.0 update 6 or above (v4.3 update 6 for Mac OS X)
 * Any MPI implementation with `MPI_THREAD_SERIALIZED` support for threading.
 * [Boost](http://www.boost.org) v1.41 or above
 * Optional: [Doxygen](http://www.doxygen.org) for building its documentation

### Setting up Intel TBB </p>


Intel TBB must be configured before building or using Dparallel_recursion. This requires setting up some environment variables both to be compiled and to be used by an application during its execution. This usually requires executing the script 
 
 - `${TBBROOT}/bin/tbbvars.sh` if your shell is `bash`/`ksh`, or
 - `${TBBROOT}/bin/tbbvars.csh` if your shell is `csh`

 where `${TBBROOT}` is the directory where TBB has been installed. The file includes the definition of the variable `TBBROOT`, which must point to the directory where TBB has been installed, but it is usually left empty during the installation. For this reason right after intalling TBB you must edit this file to define this variable. In order to do this, open the file and look for a line like
 
        export TBBROOT="SUBSTITUTE_INSTALL_DIR_HERE"

 in the case of `tbbvars.sh` or 

        setenv TBBROOT "SUBSTITUTE_INSTALL_DIR_HERE"
	  
 in the case of `tbbvars.csh` and put the root directory for your TBB installation between the double quotes. 


### Step by step procedure 

1. Make sure TBBs are automatically found by your compiler following the steps described above.

2. Unpack the dparallel_recusion tarball (e.g. `dparallel_recusion.tar.gz`)

		tar -xzf dparallel_recusion.tar.gz
		cd dparallel_recusion		
 or clone the project from its repository
 
	    git clone git@github.com:fraguela/dparallel_recusion.git
	
3. Create the temporary directory where the project will be built and enter it :

		mkdir build && cd build

4. Generate the files for building the benchmaks and tests for the library in the format that you prefer (Visual Studio projects, nmake makefiles, UNIX makefiles, Mac Xcode projects, ...) using cmake.

    In this process you can use a graphical user interface for cmake such as `cmake-gui` in Unix/Mac OS X or `CMake-gui` in Windows, or a command-line interface such as `ccmake`. The process is explained here assuming this last possibility, as graphical user interfaces are not always available.
 
5. run `ccmake ..`
	
	 This will generate the files for building Dparallel_recusion with the tool that cmake choses by default for your platform. Flag `-G` can be used to specify the kind of tool that you want to use. For example if you want to use Unix makefiles but they are not the default in you system, run <tt>ccmake -G 'Unix Makefiles' ..</tt>

	 Run `ccmake --help` for additional options and details.

6. Press letter `c` to configure your build.

7. Provide the values you wish for the variables that appear in the screen. The most relevant ones are:
	- `CMAKE_BUILD_TYPE` : String that specifies the build type. Its possible values are empty, Debug, Release, RelWithDebInfo and MinSizeRel.

	   Dparallel_recursion is a header-only library, and thus it does not need any building. The flag controls the type of build desired for the tests and benchmarks delivered with the library.
	  
	- `CMAKE_INSTALL_PREFIX` : Directory where Dparallel_recusion will be installed.

8. When you are done, press `c` to re-configure cmake with the new values.
9. Press `g` to generate the files that will be used to build the benchmarks and tests for the library and exit cmake.
10. The rest of this explanation assumes that UNIX makefiles were generated in the previous step. 

	Run `make` in order to build the benchmarks and the tests.  The degree of optimization, debugging information and assertions enabled depends on the value you chose for variable `CMAKE_BUILD_TYPE`.
	
    You can use the flag `-j` to speedup the building process. For example, `make -j4` will use 4 parallel processes, while `make -j` will use one parallel process per available hardware thread.
11. (Optionally) run `make tests` in order to run the Dparallel_recusion tests. 
12. Run `make install` 

    This installs Dparallel_recursion under the directory you specified for the `CMAKE_INSTALL_PREFIX` variable. If you left it empty, the default base directories will be `/usr/local` in Unix and `c:/Program Files` in Windows. 

    Since, as commented above, Dparallel_recusion is a header-only library, it is installed as a set of header files located in the directory `CMAKE_INSTALL_PREFIX/include/dparallel_recursion`.

    The binaries of the benchmarks and the tests are installed in the directory `bin` within the directory where Dparallel_recursion was unpacked or cloned so that one can play with them, but they are not installed inside `CMAKE_INSTALL_PREFIX`, as they are not needed.

13. You can remove the `dparallel_recursion` directory generated by the unpacking of the tarball or the cloning of the project repository.
