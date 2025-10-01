CXX = g++
CXXFLAGS =  -std=c++20 -Wall -Wextra -O3 -ffast-math -flto -march=native -mtune=native -fomit-frame-pointer -finline-limit=500
PERFFLAGS = -e task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses
FLAME_PATH := ${HOME}/main/FlameGraph
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

all: test

test: tests.cpp
	$(CXX) -std=c++20 -Wall -Wextra -g -o tests tests.cpp engine.cpp
	./tests
	
benchmark: engine.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	./lll-bench $(MAKEFILE_DIR)engine.so -d 1

perf:
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	perf stat ${PERFFLAGS} ./lll-bench $(MAKEFILE_DIR)engine.so -d 1 

flame:
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	perf record -F 99 -g -a ./lll-bench $(MAKEFILE_DIR)engine.so -d 1
	perf script | ${FLAME_PATH}/stackcollapse-perf.pl | ${FLAME_PATH}/flamegraph.pl > flamegraph.svg

clean:
	rm -f tests engine.o engine.so script
