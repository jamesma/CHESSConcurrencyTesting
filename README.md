CHESSConcurrencyTesting
=======================

Author: Jun Yan Ma (ma23@purdue.edu)

Project link: http://www.cs.purdue.edu/homes/kim1051/cs490/proj2/p2.html

CHESS program containing pthread function hooks is located at `chess.cpp`.

CHESS tool is located at `chesstool.cpp`.

Finding Atomicity Violations (Part 2)
=====================================

`sample2.c` has an atomicity violation bug and it will crash the program in a certain schedule.

1. Compile the `sample2` binary by giving our `chess.so` shared object precedence: `make eg source=sample2`

2. Ensure that `chess.cpp` has been built: `make chess.so`

3. Compile the CHESS tool binary: `make chesstool`

4. At last, we now have the CHESS tool binary `chesstool`.

To check `sample2` for atomicity violations, run the CHESS tool with the proper arguments: `./chesstool sample2`

The CHESS tool will now execute the program by first finding all synchronization points and writing them to a dotfile `.tracksyncpts`. Then it will iterate over all of the synchronization points. If the program crashes during an execution, CHESS tool will make a note of it and continue to the next iteration. 

After all iterations have been executed, a crash report will be printed at the end, detailing the synchronization point that an execution has failed.

If concurrency errors (atomicity violation in particular) are not detected, a cookie is given. :smile:

In-Depth Explanation of Implementation
======================================

Initialization of CHESS tool will write `0/0` into file `.tracksyncpts`. `chess.cpp` will read it and detect that this is the first execution and computes the total number of synchronization points of the program. For example, if `sample2` has 49 synchronization points, at the end of this first execution, `.tracksyncpts` will contain `1/49`. What this means is that we are at the 1st execution out of a total of 49 executions.

Further running `sample2` using `./run.sh sample2` will read in from `.tracksyncpts` file and accordingly switch thread at the current Nth synchronization point. The current execution counter is increased at every thread switch.

The CHESS program is designed to reset the Nth execution back to 1 when it has reached its total number of executions.