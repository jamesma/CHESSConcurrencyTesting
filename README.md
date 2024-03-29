CHESSConcurrencyTesting
=======================

Author: Jun Yan Ma (ma23@purdue.edu)

Project description: ProjectDescription.pdf

CHESS program containing pthread function hooks: `chess.cpp`.

CHESS tool: `chesstool.cpp`.

Finding Atomicity Violations (Part 2)
=====================================

`sample2.c` has an atomicity violation bug and it will crash the program in a certain schedule.

1. Compile the `sample2` binary by giving our `chess.so` shared object precedence: `make eg source=sample2`

2. Ensure that `chess.cpp` has been built: `make chess.so`

3. Compile the CHESS tool binary: `make chesstool`

At last, we now have the CHESS tool binary `chesstool`. To check `sample2` for atomicity violations, run the CHESS tool with the proper arguments: `./chesstool sample2`

The CHESS tool will now execute the program by finding all synchronization points and writing them to a dotfile `.tracksyncpts`. After that, it will iterate over all of the synchronization points. If the program crashes during an execution, CHESS tool will make a note of it and continue to the next iteration. 

After all iterations have been executed, a crash report will be printed at the end, detailing the synchronization points where execution has failed.

If no concurrency errors (atomicity violation in particular) are detected, a cookie is given. :smile:

In-Depth Explanation of Implementation
======================================

Initialization of CHESS tool will write `0/0` into file `.tracksyncpts`. `chess.cpp` will read it and detect that this is the first execution and computes the total number of synchronization points of the program. For example, if `sample2` has 49 synchronization points, at the end of this first execution, `.tracksyncpts` will contain `1/49`. What this means is that we are at the 1st execution out of a total of 49 executions.

Further running `sample2` using `./run.sh sample2` will read in from `.tracksyncpts` file and accordingly switch thread at the current Nth synchronization point. The current execution counter is increased at every thread switch.

The CHESS program is designed to reset the Nth execution back to 1 when it has reached its total number of executions.