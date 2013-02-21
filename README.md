Introduction
==============

The purpose of this lab is to write a custom dynamic storage allocator for C programs, i.e., our own version of the malloc, free, and realloc routines. The overall goal is to implement a correctly working, fast, and efficient allocator.

Logistics
==============

Trace-Driven Driver Program
=============================

The driver program mdriver.c in the source code distribution tests the mm.c package for correctness, space utilization, and throughput. Each trace file contains a sequence of allocate, reallocate, and free directions that instruct the driver to call the mm_malloc, mm_realloc, and mm_free routines in some sequence.

The driver mdriver.c accepts the following command line arguments:

./mdriver -t <tracedir> -f <tracefile> -h -I -v -V -d n
