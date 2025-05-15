CXX = g++
CXXFLAGS =  -std=c++20 -Wall -Wextra -Ofast -ffast-math -flto -march=native -mtune=native -fomit-frame-pointer -fprefetch-loop-arrays -fno-plt -finline-functions -finline-limit=500
PERFFLAGS = -B -e task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-icache-loads,L1-icache-load-misses,LLC-loads,LLC-load-misses
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
all: test

script: script.cpp
	$(CXX) -std=c++20 -Wall -Wextra -g -o script script.cpp engine.cpp

	
test: tests.cpp
	$(CXX) -std=c++20 -Wall -Wextra -g -g -o tests tests.cpp engine.cpp
	./tests

submit: engine.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	DATE_SUFFIX=$(shell date +%m%d_%H%M%S); \
	BENCHMARK_FILE=benchmarks/results_$$DATE_SUFFIX.txt; \
	perf stat $(PERFFLAGS) lll-bench $(MAKEFILE_DIR)engine.so -d 1 2>&1 | tee -a "$$BENCHMARK_FILE"

benchmark:
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	perf stat $(PERFFLAGS) lll-bench $(MAKEFILE_DIR)engine.so -d 1 

record:
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	DATE_SUFFIX=$(shell date +%m%d_%H%M%S); \
	BENCHMARK_FILE=records/records_$$DATE_SUFFIX.data; \
	perf record -F 100 $(PERFFLAGS) -o $$BENCHMARK_FILE lll-bench $(MAKEFILE_DIR)engine.so -d 1

flame:
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	perf record -F 99 -g -a lll-bench $(MAKEFILE_DIR)engine.so -d 1
	perf script | ${HOME}/main/FlameGraph/stackcollapse-perf.pl | ${HOME}/main/FlameGraph/flamegraph.pl > flamegraph.svg

callgrind:
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	valgrind --tool=callgrind lll-bench $(MAKEFILE_DIR)engine.so -d 1
	cg_annotate callgrind.out.*

clean:
	rm -f tests engine.o engine.so script