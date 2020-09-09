# Parallelism Abstraction Layer
This directory contains old (SimpleSSD version < v2.1) NAND model, called Parallelism Abstraction Layer (PAL).
As I rewrite all simulation model in event-driven fasion, PAL becomes huge overhead on simulation performance (because it maintains timeline for functional simulator).
So, I replaced PAL with FSM-style NAND model which only stores current state of NAND flash.
PAL is not maintained anymore, but archived here.

## Enable PAL again?
If you want to try PAL:

1. Add all `*.cc` files to `CMakeFiles.txt`.
2. Initiate `PALOLD` class in `fil/fil.cc`.

Checkout to the `v2.1-beta6` for the working source code.
