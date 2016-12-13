## Dparallel_recursion </p>

This library provides a framework to parallelize algorithms that follow a divide-and-conquer pattern. Its two main components are the skeletons `parallel_recursion` and `dparallel_recursion`. The first one parallelizes the required algorithm using multithreading within a single process. The second skeleton supports the parallelization across hybrid memory environments such as clusters of nodes with multi-core processors, as it supports also multiple processes on top of the MPI standard.

### Examples and documentation

- The `benchmarks` and `tests` directories contain implementations of algorithms with very different nature using the library and execising all its features.

- Running `make doc` in the `doc directory` builds a user-level documenation based on Doxygen. A more detailed documentation can be built by running `make internal-doc`.

- The `parallel_recursion` skeleton and its supporting classes are described and compared with other alternatives in the publication [A Generic Algorithm Template for Divide-and-conquer in Multicore Systems](http://www.des.udc.es/~basilio/papers/hpcc10.pdf) ([DOI 10.1109/HPCC.2010.24](http://dx.doi.org/10.1109/HPCC.2010.24)).

- A paper describing the `dparallel_recursion` skeleton is currently under review.

### License

This library is licensed under the [Apache license V2](http://www.apache.org/licenses/) because [that is the license for the Intel® TBB](https://www.threadingbuildingblocks.org/how-tbb-licensed) threading system it relies on. If you change it to rely on any other threading system, feel free to adapt the license accordingly.
