
CXX      := mpicxx
CXXFLAGS := -fopenmp -O3 -DNDEBUG


all: treeadd

%.o : %.cpp tree.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

treeadd: node.o par-alloc.o args.o
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f *.o treeadd
