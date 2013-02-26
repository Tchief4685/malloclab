Introduction
==============

The purpose of this lab is to write a custom dynamic storage allocator for C programs, i.e., our own version of the malloc, free, and realloc routines. The overall goal is to implement a correctly working, fast, and efficient allocator.

Logistics
==============

The dynamic storage allocator will consist of the following four functions, which are declared in mm.h and defined in mm.c: (1) int mm_init(void); (2) void *mm_malloc(size_t size); (3) void mm_free(void *ptr); (4) void *mm_realloc(void *ptr, size_t size);

The semantics outlined in the handout match the semantics of the corresponding libc malloc, realloc, and free routines. Type man malloc to the shell for complete documentation.

Trace-Driven Driver Program
=============================

The driver program mdriver.c in the source code distribution tests the mm.c package for correctness, space utilization, and throughput. Each trace file contains a sequence of allocate, reallocate, and free directions that instruct the driver to call the mm_malloc, mm_realloc, and mm_free routines in some sequence.

The driver mdriver.c accepts the following command line arguments:

./mdriver -t <tracedir> -f <tracefile> -h -I -v -V -d n
