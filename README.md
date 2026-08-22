# C Programming Course Examples

A comprehensive set of example operations for a C programming course, taking
students from beginner to advanced. All examples are implemented in a single
file, `main.c`, organized into twelve chapters:

1. **Fundamentals** — data types, operators, control flow, loops
2. **Functions** — parameters, return values, recursion (factorial, Fibonacci, GCD, primes)
3. **Arrays & Strings** — aggregation, reversal, hand-written `strlen`/`strcpy`/`strcmp`/`strcat`, palindromes, word counting
4. **Matrices** — 2D arrays, addition, multiplication, transpose
5. **Pointers** — dereferencing, pointer arithmetic, pointer-to-pointer, function pointers and dispatch tables
6. **Structs, Enums, Unions** — records, tagged unions, designated initializers
7. **Dynamic Memory** — `malloc`/`calloc`/`realloc`/`free`, a growable vector
8. **Data Structures** — linked list, stack, queue, binary search tree, hash table
9. **Algorithms** — linear/binary search; bubble, selection, insertion, merge sort; `qsort`
10. **File I/O** — text files (write/append/read) and binary struct round-trips
11. **Bit Manipulation** — set/clear/toggle/test bits, popcount, power-of-two tests
12. **Advanced Topics** — variadic functions, macros, `static` state, error handling, generic (`void *`) programming

Every function is documented in [DOCUMENTATION.md](DOCUMENTATION.md), with
explanations of the concepts, idioms, and pitfalls it teaches.

## Building and running

Requires `gcc` (or any C11 compiler) and `make`.

```bash
make          # build the `main` binary
./main        # run all chapters in order
make run      # build and run in one step
make debug    # rebuild with -g -O0 for debugging with gdb
make clean    # remove the binary and demo output files
```

The build uses `-std=c11 -Wall -Wextra -pedantic` and compiles warning-free.

## Layout

| File | Purpose |
|---|---|
| `main.c` | All example implementations, organized by chapter |
| `DOCUMENTATION.md` | Written explanation of every function |
| `Makefile` | Builds the `main` binary |
