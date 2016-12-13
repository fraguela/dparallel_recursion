
CXX      := mpicxx
CXXFLAGS := -fopenmp -O3 -DNDEBUG

# EP is ignored by now
LOCALBINARIES := fib NQueens quicksort quicksort2

all: $(LOCALBINARIES) treeadd tsp

fib: fib.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

NQueens: NQueens.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

quicksort: quicksort.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

quicksort2: quicksort2.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

treeadd:
	$(MAKE) --no-print-directory -C treeadd

tsp:
	$(MAKE) --no-print-directory -C tsp

clean:
	rm -f *.o $(LOCALBINARIES) treeadd/treeadd treeadd/*.o tsp/tsp tsp/*.o
