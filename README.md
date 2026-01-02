## Dparallel_recursion </p>
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![DOI:10.1007/s11227-021-04259-5](https://img.shields.io/badge/DOI-10.1007/s11227--021--04259--5-blue)](https://doi.org/10.1007/s11227-021-04259-5)

This library provides a high level framework to parallelize algorithms that follow a divide-and-conquer pattern.

### Installation

 Please read the accompanying `INSTALL.md` file.
 
### Examples and documentation

- The `benchmarks` and `tests` directories contain implementations of algorithms with very different nature using the library and execising all its features.

- Running `make doc` in the `doc directory` builds a user-level documenation based on Doxygen. A more detailed documentation can be built by running `make internal-doc`.

- The `parallel_recursion` skeleton and its supporting classes parallelize the required algorithm using multithreading within a single process. They are described and compared with other alternatives in the publication [A Generic Algorithm Template for Divide-and-conquer in Multicore Systems](http://www.des.udc.es/~basilio/papers/hpcc10.pdf) ([DOI 10.1109/HPCC.2010.24](http://dx.doi.org/10.1109/HPCC.2010.24)).

- The `dparallel_recursion` skeleton supports the parallelization across hybrid memory environments such as clusters of nodes with multi-core processors as well as multiple processes on top of the MPI standard. It is described and compared with other alternatives in the publication [A general and efficient divide-and-conquer algorithm framework for multi-core clusters](http://www.des.udc.es/~basilio/papers/Gonzalez17-cluster.pdf) ([DOI 10.1007/s10586-017-0766-y](http://dx.doi.org/10.1007/s10586-017-0766-y)).

- The `parallel_stack_recursion` skeleton allows a more efficient, robust and portable parallelization of divide-and-conquer patterns in shared memory than `parallel_recursion`. It is described and compared with other alternatives in the publication [A Parallel Skeleton for Divide-and-conquer Unbalanced and Deep Problems](http://www.des.udc.es/~basilio/papers/Martinez21-DaC.pdf) ([DOI 10.1007/s10766-021-00709-y](http://dx.doi.org/10.1007/s10766-021-00709-y)).

- The `dparallel_stack_recursion` skeleton supports the generic high-performance and robust parallelization of divide-and-conquer algorithms supporting hybrid shared-distributed memory parallelism providing strong load-balancing features. It is described and compared with other alternatives in the publication [A highly optimized skeleton for Unbalanced and Deep Divide-and-conquer Algorithms on Multi-core Clusters](http://dx.doi.org/10.1007/s11227-021-04259-5) ([DOI 10.1007/s11227-021-04259-5](http://dx.doi.org/10.1007/s11227-021-04259-5))

### License

This library is licensed under the [Apache license V2.0](https://opensource.org/licenses/Apache-2.0) because [that is the license for the Intel® TBB](https://www.threadingbuildingblocks.org/how-tbb-licensed) threading system on which some parts of it rely on. If you change it to rely on any other threading system, feel free to adapt the license accordingly.
