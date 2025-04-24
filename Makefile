CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 
PERFFLAGS = -B -e task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-icache-loads,L1-icache-load-misses,LLC-loads,LLC-load-misses
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
all: test

script: script.cpp
	$(CXX) $(CXXFLAGS) -g -o script script.cpp engine.cpp
	
test: tests.cpp
	$(CXX) $(CXXFLAGS) -o tests tests.cpp engine.cpp
	./tests

submit: engine.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	DATE_SUFFIX=$(shell date +%m%d_%H%M%S); \
	BENCHMARK_FILE=benchmarks/results_$$DATE_SUFFIX.txt; \
	lll-bench $(MAKEFILE_DIR)engine.so -d 1 >> "$$BENCHMARK_FILE"

benchmark:
	perf stat $(PERFFLAGS) lll-bench $(MAKEFILE_DIR)engine.so -d 1 

clean:
	rm -f tests engine.o engine.so script
