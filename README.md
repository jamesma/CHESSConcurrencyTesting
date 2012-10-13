CHESSConcurrencyTesting
=======================

Author: Jun Yan Ma (ma23@purdue.edu)

Project link: http://www.cs.purdue.edu/homes/kim1051/cs490/proj2/p2.html

Finding Atomicity Violations
============================

`sample2.c` has an atomicity violation bug and it will crash the program in a certain schedule.

First, run the chess algorithm to explore the number of synchronization points: `make chessinit source=sample2`

This will write `0/0` into file `.tracksyncpts`. `chess.cpp` will read it and detect that this is the first execution and computes the total number of synchronization points of the program accordingly. For example, if `sample2` has 49 synchronization points, at the end of this first execution, `.tracksyncpts` will contain `1/49`. What this means is that we are at the 1st execution out of a total of 49 executions.

Further running `sample2` using `./run.sh sample2` will read in from `.tracksyncpts` file and accordingly switch thread at the current Nth synchronization point.

The CHESS program has been designed to reset the Nth execution back to 1 when it has reached its total number of executions.