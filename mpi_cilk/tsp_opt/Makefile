
CXX      := mpicxx
CXXFLAGS := -fopenmp -O3 -DNDEBUG


all: tsp

%.o : %.cpp tsp.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

tsp: main.o build.o tsp.o args.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lboost_serialization

clean:
	rm -f *.o tsp
