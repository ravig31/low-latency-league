======================================
=========== Add_order stats ==========
======================================
CPU Cycles: 1244168
L1 Cache Misses: 0
L2 Cache Misses: 0
Branch Mispredictions: 31
Branch Misprediction Rate: 0.00256189

======================================
========== Modify_order stats ========
======================================
CPU Cycles: 612633
L1 Cache Misses: 0
L2 Cache Misses: 0
Branch Mispredictions: 15
Branch Misprediction Rate: 0.0025604

======================================
========== Get_volume stats ==========
======================================
CPU Cycles: 274232
L1 Cache Misses: 0
L2 Cache Misses: 0
Branch Mispredictions: 7
Branch Misprediction Rate: 0.00256169


Running competition benchmark (60% add_order, 40% modify_order, 20% get_level)...
======================================
=========== Combined stats ===========
======================================
CPU Cycles: 1325121
L1 Cache Misses: 0
L2 Cache Misses: 0
Branch Mispredictions: 33
Branch Misprediction Rate: 0.00256023


 Performance counter stats for './lll-bench /home/ravig31/main/low-latency-league/engine.so -d 1':

          4,209.55 msec task-clock:u                     #    0.999 CPUs utilized
                 0      context-switches:u               #    0.000 /sec
                 0      cpu-migrations:u                 #    0.000 /sec
             1,281      page-faults:u                    #  304.308 /sec
    15,141,661,795      cycles:u                         #    3.597 GHz                         (20.04%)
    31,329,938,666      instructions:u                   #    2.07  insn per cycle              (20.04%)
     6,808,980,843      branches:u                       #    1.618 G/sec                       (20.05%)
       169,549,893      branch-misses:u                  #    2.49% of all branches             (20.07%)
       566,879,920      cache-references:u               #  134.665 M/sec                       (20.10%)
         1,607,402      cache-misses:u                   #    0.28% of all cache refs           (20.09%)
    15,916,252,112      L1-dcache-loads:u                #    3.781 G/sec                       (20.07%)
       491,318,590      L1-dcache-load-misses:u          #    3.09% of all L1-dcache accesses   (20.06%)
         8,350,388      L1-icache-loads:u                #    1.984 M/sec                       (20.06%)
            63,684      L1-icache-load-misses:u          #    0.76% of all L1-icache accesses   (20.06%)
   <not supported>      LLC-loads:u
   <not supported>      LLC-load-misses:u

       4.211853612 seconds time elapsed

       4.150925000 seconds user
       0.008864000 seconds sys
