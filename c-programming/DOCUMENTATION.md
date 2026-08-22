# C Programming Course — Documentation

This document explains every function implemented in `main.c`. The code is
organized as twelve **chapters** that take a student from complete beginner to
advanced C programmer. Each chapter's demo functions are called in order from
`main`, so running the program (`make run`) walks through the whole course.

Build and run:

```bash
make        # builds the `main` binary
./main      # run it (or `make run`)
make debug  # rebuild with -g -O0 for use with gdb
make clean  # remove the binary and demo output files
```

---

## Table of Contents

1. [Chapter 1 — Fundamentals](#chapter-1--fundamentals)
2. [Chapter 2 — Functions & Recursion](#chapter-2--functions--recursion)
3. [Chapter 3 — Arrays & Strings](#chapter-3--arrays--strings)
4. [Chapter 4 — Matrices (2D Arrays)](#chapter-4--matrices-2d-arrays)
5. [Chapter 5 — Pointers](#chapter-5--pointers)
6. [Chapter 6 — Structs, Enums, Unions](#chapter-6--structs-enums-unions)
7. [Chapter 7 — Dynamic Memory](#chapter-7--dynamic-memory)
8. [Chapter 8 — Data Structures](#chapter-8--data-structures)
9. [Chapter 9 — Algorithms: Searching & Sorting](#chapter-9--algorithms-searching--sorting)
10. [Chapter 10 — File I/O](#chapter-10--file-io)
11. [Chapter 11 — Bit Manipulation](#chapter-11--bit-manipulation)
12. [Chapter 12 — Advanced Topics](#chapter-12--advanced-topics)

---

## Preprocessor Macros

Defined at the top of `main.c` and used throughout.

### `ARRAY_LEN(a)`

```c
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
```

Computes the number of elements in a statically-sized array by dividing the
total byte size by the size of one element. **Only valid on true arrays** in
the same scope where they were declared — once an array is passed to a
function it decays to a pointer and `sizeof` returns the pointer size instead.

### `MAX(a, b)` / `MIN(a, b)`

Classic function-like macros. Every argument and the entire expansion are
parenthesized to avoid operator-precedence bugs (e.g. `MAX(x & 1, y)` would
mis-parse without them). Teaching point: macro arguments are evaluated each
time they appear, so `MAX(i++, j)` increments `i` once or twice — never pass
expressions with side effects to macros.

### `CHAPTER(title)`

Prints a banner separating each demo's output. Demonstrates multi-line macros
and adjacent string-literal concatenation.

---

## Chapter 1 — Fundamentals

### `demo_data_types(void)`

Declares one variable of each fundamental scalar type (`char`, `int`,
`unsigned int`, `long`, `long long`, `float`, `double`, `bool`) and prints its
value and platform size via `sizeof`. Students learn:

- Each type's printf conversion specifier (`%c`, `%d`, `%u`, `%ld`, `%lld`,
  `%f`, `%zu` for `size_t`).
- Literal suffixes (`42u`, `1234L`, `3.14f`).
- `bool` comes from `<stdbool.h>` (C99); `INT_MIN`/`INT_MAX` from `<limits.h>`.
- Sizes are platform-dependent; only relationships are guaranteed by the
  standard (e.g. `sizeof(long long) >= 8`).

### `demo_operators(void)`

Covers the operator families:

- **Arithmetic** `+ - * / %`, including the crucial difference between integer
  division (`17 / 5 == 3`, truncated) and real division (`(double)17 / 5 ==
  3.4`, forced by a cast).
- **Comparison** `> == !=` produce `int` 0 or 1.
- **Logical** `&& || !` with short-circuit evaluation.
- **Increment**: postfix `x++` evaluates to the *old* value, prefix `++x` to
  the *new* value. The demo deliberately computes each into a separate
  variable first, because mixing `x++` and `x` inside one expression is
  *undefined behavior* (unsequenced modification) — an explicit lesson.
- **Compound assignment** `+= -= *= /=`.
- **Ternary** `cond ? a : b` as an expression-level if/else.

### `classify_number(int n)` → `const char *`

Returns `"negative"`, `"zero"`, `"positive even"` or `"positive odd"`.
Demonstrates the `if / else if / else` chain and returning string literals
(which have static storage duration, so returning them is safe).

### `weekday_name(int day)` → `const char *`

Maps 1–7 to day names with a `switch`. Shows `case` labels, `return` instead
of `break` (each case exits immediately), and a `default` arm that handles
invalid input rather than ignoring it.

### `demo_control_flow(void)`

Drives the two functions above over sample inputs.

### `demo_loops(void)`

All three loop statements plus flow control:

- `for` — counted iteration, the most common loop.
- `while` — condition checked *before* each iteration.
- `do-while` — condition checked *after*, so the body always runs at least once.
- `break` — exits the innermost loop immediately.
- `continue` — skips the rest of the body and re-tests the condition.
- Nested loops — a 3×3 multiplication table, showing row/column iteration.

---

## Chapter 2 — Functions & Recursion

### `add(int a, int b)` → `int`

The minimal function: parameters, a return value, and the key fact that C
passes **all arguments by value** — the function receives copies and cannot
change the caller's variables.

### `factorial_recursive(int n)` → `long long`

Computes `n!` by the definition `n! = n × (n−1)!` with base case `0! = 1! = 1`.
The base case is what stops the recursion; without it the stack overflows.
Returns `long long` because `13!` already overflows 32-bit `int`.

### `factorial_iterative(int n)` → `long long`

The same computation as a loop. Comparing it with the recursive version
teaches that any recursion can be rewritten iteratively, usually with less
overhead (no call stack growth).

### `fibonacci(int n)` → `long long`

The n-th Fibonacci number computed iteratively with two rolling variables
(`prev`, `curr`) in O(n) time. Contrast with naive double recursion, which is
exponential — a first taste of algorithmic complexity.

### `gcd(int a, int b)` → `int`

Euclid's algorithm: repeatedly replace `(a, b)` with `(b, a mod b)` until `b`
is zero. One of the oldest known algorithms and a beautiful example of a loop
invariant.

### `int_power(int base, unsigned exponent)` → `long long`

Raises `base` to a non-negative power by repeated multiplication —
accumulating a result across loop iterations.

### `is_prime(int n)` → `bool`

Trial division testing odd divisors up to √n (as `d * d <= n`, avoiding
floating point). Rests on the theorem that a composite number must have a
factor no greater than its square root. O(√n).

### `demo_functions(void)`

Calls each function above with sample inputs and prints the primes below 30.

---

## Chapter 3 — Arrays & Strings

**Core concept:** when an array is passed to a function it *decays* into a
pointer to its first element, and its length is lost. Every array function
therefore takes an explicit `size_t len` parameter.

### `array_sum(const int *arr, size_t len)` → `long`

Accumulates all elements. The `long` return prevents overflow for large
arrays; `const` promises the function will not modify the array.

### `array_max` / `array_min` `(const int *arr, size_t len)` → `int`

Track the best-so-far value across a single pass. Both assume `len >= 1`
(there is no maximum of an empty set).

### `array_average(const int *arr, size_t len)` → `double`

Reuses `array_sum` and casts before dividing so the division is done in
floating point, not truncated integer arithmetic.

### `array_reverse(int *arr, size_t len)`

In-place reversal with the **two-pointer technique**: indices `i` and `j`
start at the ends, swap, and move toward each other until they cross. O(n)
time, O(1) extra space.

### `print_int_array(const int *arr, size_t len)`

Utility that prints `[a, b, c]`, handling the no-trailing-comma edge case.
Used by many later demos.

### String functions

C strings are just `char` arrays terminated by a `'\0'` byte. To cement this,
four standard functions are re-implemented from scratch:

| Function | Mirrors | Key idea |
|---|---|---|
| `my_strlen(const char *s)` | `strlen` | count until the `'\0'` terminator |
| `my_strcpy(char *dst, const char *src)` | `strcpy` | copy including `'\0'`; the idiom `while ((*dst++ = *src++))` copies inside the condition. Caller must size `dst` — C never checks |
| `my_strcmp(const char *a, const char *b)` | `strcmp` | walk while equal; return negative/zero/positive difference of the first mismatching bytes (as `unsigned char`, per the standard) |
| `my_strcat(char *dst, const char *src)` | `strcat` | seek `dst`'s terminator, then copy `src` there |

### `string_reverse(char *s)`

The two-pointer swap applied to characters; reverses in place.

### `is_palindrome(const char *s)` → `bool`

Case-insensitive palindrome test that skips non-alphanumeric characters
(`"Racecar!"` → true). Uses `isalnum`/`tolower` from `<ctype.h>`; the casts to
`unsigned char` before calling them avoid undefined behavior when `char` is
signed and holds a negative value.

### `count_vowels(const char *s)` → `size_t`

Iterates with a pointer and uses `strchr("aeiouAEIOU", *p)` for set
membership — a handy standard-library trick.

### `string_to_upper(char *s)`

Uppercases in place with `toupper`, again with the `unsigned char` cast.

### `count_words(const char *s)` → `size_t`

Counts whitespace-separated words by detecting *transitions* from whitespace
into a word — a miniature two-state state machine (`in_word` flag).

---

## Chapter 4 — Matrices (2D Arrays)

A 2D array parameter must declare every dimension except the first
(`int m[MAT_N][MAT_N]`), because the compiler needs the column count to
compute element addresses: C stores matrices in **row-major** order, so
element `[r][c]` lives at offset `r * cols + c`.

> Note: the matrix parameters are deliberately *not* `const` — before C23,
> passing a non-const 2D array to a `const int [N][N]` parameter triggers a
> pedantic warning. A well-known quirk worth teaching.

### `matrix_print(int m[MAT_N][MAT_N])`

Prints the matrix with aligned columns (`%4d` field width).

### `matrix_add(a, b, out)`

Element-wise addition: `out[r][c] = a[r][c] + b[r][c]`. O(n²).

### `matrix_multiply(a, b, out)`

Textbook O(n³) multiplication: each output cell is the dot product of a row
of `a` with a column of `b`, computed by the innermost `k` loop. The demo
multiplies by the identity matrix so students can verify the result equals
the input.

### `matrix_transpose(in, out)`

Flips over the main diagonal: `out[c][r] = in[r][c]`.

---

## Chapter 5 — Pointers

### `swap(int *a, int *b)`

*The* canonical pointer example. Because C is pass-by-value, a function can
only modify its caller's variables if the caller passes their **addresses**.
`swap` dereferences both pointers and exchanges the pointed-at values through
a temporary.

### `find_max_ptr(int *arr, size_t len)` → `int *`

Returns a *pointer to* the largest element instead of its value. The caller
can then both read it and write through it (the demo zeroes the maximum), and
compute its index with pointer subtraction (`max - arr`, printed with `%td`).

### `demo_pointers(void)`

Walks through the pointer toolbox:

- `&` (address-of) and `*` (dereference); printing addresses with `%p` and a
  `(void *)` cast.
- Writing through a pointer changes the original variable.
- **Pointer arithmetic**: `arr[i]` is by definition `*(arr + i)`; the demo
  iterates an array via `*(p + i)`.
- **Pointer to pointer** (`int **pp`): two dereferences reach the value.
- **NULL**: the "points at nothing" sentinel; always check before
  dereferencing.

### Function pointers

`op_add`, `op_sub`, `op_mul` are trivial binary functions used as callbacks.

### `apply_operation(int a, int b, int (*op)(int, int))` → `int`

Takes *the operation itself* as a parameter. The declarator
`int (*op)(int, int)` reads "pointer to function taking two ints, returning
int". This is how C expresses higher-order functions, and the mechanism
behind `qsort` comparators and event callbacks.

### `demo_function_pointers(void)`

Calls `apply_operation` with each operation, then builds a **dispatch table**
— an array of `{name, function pointer}` structs iterated at runtime — the
foundation of command interpreters and virtual-function tables.

---

## Chapter 6 — Structs, Enums, Unions

### `Point` + `point_distance(Point a, Point b)` → `double`

A struct groups related data into one type. `point_distance` computes the
Euclidean distance √(dx² + dy²) using `sqrt` from `<math.h>` (which is why the
Makefile links `-lm`). Small structs are passed **by value** (copied) — cheap
here, and it means the function cannot alter the caller's points.

### `Student` + `student_print(const Student *s)`

A realistic record mixing a fixed-size string field (`char name[32]`), an
`int`, and a `double`. `student_print` takes a *pointer* to avoid copying and
accesses members with the arrow operator (`s->name` is `(*s).name`). The demo
also shows that string fields must be filled with `strcpy`, never `=`.

### `Color` enum + `color_name(Color c)` → `const char *`

An `enum` names integer constants (`COLOR_RED == 0`, etc.). `color_name` maps
them back to strings with a `switch` — the standard pattern for printing
enums, since C stores only the number.

### `TaggedValue` (tagged union) + `tagged_value_print`

A `union` overlays several members on the **same memory**; only the most
recently written member is valid, and the union's size is the size of its
largest member (verified in the demo output). Because C doesn't remember
which member is active, the union is wrapped in a struct with a `ValueTag`
enum — the classic **tagged union / variant** pattern. Designated
initializers (`{ .i = 42 }`) select which member to initialize.

---

## Chapter 7 — Dynamic Memory

Stack variables die when their scope ends; heap memory obtained from
`malloc`/`calloc`/`realloc` lives until explicitly `free`d. The contract:
**every allocation is checked for NULL, and every allocation has exactly one
free**.

### `make_range(size_t n)` → `int *`

Allocates an `int` array on the heap and fills it with 0…n−1. Ownership
transfers to the caller, who must `free` it. Note the idiom
`malloc(n * sizeof *arr)` — sizing off the target pointer means the code stays
correct if the element type ever changes.

### `my_strdup(const char *s)` → `char *`

Heap-duplicates a string (like POSIX `strdup`): measure with `strlen`,
allocate **length + 1** bytes (the +1 for the terminator is the most common
beginner bug), copy with `memcpy`.

### `IntVector` — a growable dynamic array

The pattern behind C++'s `vector` and Python's `list`: a struct holding the
heap buffer plus `size` (slots used) and `capacity` (slots allocated).

- **`vec_init(IntVector *v)`** — starts empty (`NULL`, 0, 0); the first push
  allocates.
- **`vec_push(IntVector *v, int value)` → `bool`** — appends one element.
  When full, capacity **doubles** via `realloc` (4, 8, 16, …), which makes
  appends amortized O(1). The `realloc` result is assigned to a *temporary*
  first: if it fails the original buffer is still valid and must not leak.
- **`vec_get(const IntVector *v, size_t index)` → `int`** — bounds-checked
  read; out-of-range access aborts with a message rather than silently
  corrupting memory.
- **`vec_free(IntVector *v)`** — frees the buffer and resets the fields to
  the empty state, so accidental reuse fails loudly instead of
  use-after-free.

### `demo_dynamic_memory(void)`

Shows `malloc` + `free`, `calloc` (which zero-initializes, unlike `malloc`),
`my_strdup`, and the vector growing from capacity 0 to 16 while absorbing ten
squares.

---

## Chapter 8 — Data Structures

### Singly linked list

```c
typedef struct ListNode { int value; struct ListNode *next; } ListNode;
```

Each node points to the next; `NULL` marks the end; the list *is* its head
pointer. Functions that change the head take `ListNode **head` — a pointer to
the caller's pointer — so insertion/removal at the front works without
special-casing in the caller.

- **`list_push_front(ListNode **head, int value)`** — O(1): new node's `next`
  becomes the old head; the head becomes the new node.
- **`list_push_back(ListNode **head, int value)`** — O(n): walk to the tail
  and link. Handles the empty-list case where the new node *is* the head.
- **`list_find(ListNode *head, int value)` → `ListNode *`** — linear scan;
  returns the first matching node or `NULL`.
- **`list_delete(ListNode **head, int value)` → `bool`** — removes the first
  match using a **pointer-to-pointer cursor** (`ListNode **cur`): head
  removal and mid-list removal become the same code path, eliminating the
  usual "previous node" bookkeeping.
- **`list_reverse(ListNode **head)`** — in-place reversal with three rolling
  pointers (`prev`, `cur`, `next`), re-pointing each node's `next` at its
  predecessor. O(n) time, O(1) space; a classic interview question.
- **`list_length` / `list_print`** — standard traversals.
- **`list_free(ListNode **head)`** — walks the list saving `next` *before*
  freeing each node (touching freed memory is undefined behavior), then
  NULLs the head.

### Stack (LIFO, array-backed)

Fixed capacity, `top` = index of the next free slot. All operations O(1).

- **`stack_init`** — `top = 0` (empty).
- **`stack_is_empty` / `stack_is_full`** — state predicates.
- **`stack_push(Stack *s, int value)` → `bool`** — refuses on overflow
  instead of writing out of bounds.
- **`stack_pop(Stack *s, int *out)` → `bool`** — delivers the removed value
  through an out-parameter and reports underflow via the return value — the
  idiomatic C way to return "a value or failure".
- **`stack_peek(const Stack *s, int *out)` → `bool`** — reads the top without
  removing it.

The demo pushes 11/22/33/44 and pops them back in reverse (LIFO) order.

### Queue (FIFO, circular buffer)

`head` is the dequeue position; elements enqueue at
`(head + size) % QUEUE_CAPACITY`. The modulo wrap lets the buffer be reused
endlessly without shifting elements — O(1) at both ends.

- **`queue_init` / `queue_is_empty` / `queue_is_full`** — bookkeeping.
- **`queue_enqueue(Queue *q, int value)` → `bool`** — add at the back.
- **`queue_dequeue(Queue *q, int *out)` → `bool`** — remove from the front,
  advancing `head` with wraparound.

### Binary search tree

```c
typedef struct TreeNode { int value; struct TreeNode *left, *right; } TreeNode;
```

Invariant: everything in the left subtree is smaller, everything right is
larger. Search and insert are O(log n) when balanced, degrading to O(n) for
sorted input (motivating AVL/red-black trees as a follow-on topic).

- **`bst_insert(TreeNode *root, int value)` → `TreeNode *`** — recursively
  descends to the correct empty slot and returns the (possibly new) subtree
  root, making the recursion elegant:
  `root->left = bst_insert(root->left, v)`. Duplicates are ignored.
- **`bst_contains(const TreeNode *root, int value)` → `bool`** — iterative
  descent choosing left or right at each node — binary search embodied as a
  data structure.
- **`bst_print_inorder(const TreeNode *root)`** — left, node, right. For a
  BST this visits values **in sorted order**, which the demo shows.
- **`bst_height(const TreeNode *root)` → `int`** — recursive
  `1 + max(left, right)`, empty tree = 0.
- **`bst_free(TreeNode *root)`** — post-order (children before parent) so no
  freed node is ever revisited.

### Hash table (string → int, separate chaining)

An array of `HASH_BUCKETS` linked lists ("chains"). A hash function maps each
key to a bucket; collisions simply extend that bucket's chain. Average O(1)
lookup while the load factor stays low.

- **`hash_string(const char *key)` → `unsigned long`** — the **djb2** hash:
  `h = h*33 + c` starting from 5381. Simple and effective for teaching; real
  systems use stronger functions (FNV-1a, SipHash).
- **`hash_init(HashTable *t)`** — NULLs every bucket.
- **`hash_put(HashTable *t, const char *key, int value)` → `bool`** — if the
  key already exists in its chain the value is **updated in place**;
  otherwise a new entry is prepended. The entry stores its own heap copy of
  the key (`my_strdup`) so it doesn't depend on the caller's string outliving
  the table.
- **`hash_get(const HashTable *t, const char *key, int *out)` → `bool`** —
  found/not-found is separated from the value itself (no magic sentinel
  values).
- **`hash_remove(HashTable *t, const char *key)` → `bool`** — unlinks and
  frees an entry using the same pointer-to-pointer technique as
  `list_delete`, freeing the owned key too.
- **`hash_free(HashTable *t)`** — frees every chain including key strings.

---

## Chapter 9 — Algorithms: Searching & Sorting

### `linear_search(const int *arr, size_t len, int target)` → `long`

Scan every element until a match; index or −1. O(n); the only option for
unsorted data.

### `binary_search_int(const int *arr, size_t len, int target)` → `long`

Requires **sorted** input. Maintains a half-open range `[lo, hi)` and halves
it each step: O(log n). Two important details baked in as lessons:

- `mid = lo + (hi - lo) / 2` instead of `(lo + hi) / 2`, which can overflow —
  a bug that lived in production libraries for decades.
- Half-open ranges eliminate the notorious off-by-one errors.

### `bubble_sort(int *arr, size_t len)`

Repeatedly swaps adjacent out-of-order pairs; the largest value "bubbles" to
the end each pass. The `swapped` flag ends early when a pass makes no swaps,
giving O(n) best case on sorted input; O(n²) otherwise. Taught for intuition,
not production.

### `selection_sort(int *arr, size_t len)`

Selects the minimum of the unsorted suffix and swaps it into place. Always
O(n²) comparisons but only O(n) swaps — worth contrasting with bubble sort.

### `insertion_sort(int *arr, size_t len)`

Grows a sorted prefix by inserting each element into position, shifting
larger elements right. O(n²) worst case, but excellent on small or
nearly-sorted arrays — real library sorts use it as their base case.

### `merge` / `merge_sort_range` / `merge_sort(int *arr, size_t len)` → `bool`

**Divide and conquer**: split in half, sort each half recursively, merge the
two sorted halves into a temporary buffer, copy back. Guaranteed O(n log n)
at the cost of O(n) extra memory (allocated once in `merge_sort` and threaded
through the recursion). The `<=` in the merge keeps equal elements in their
original order, making the sort **stable**. Returns `false` if the buffer
allocation fails.

### `compare_ints_asc` / `compare_ints_desc` (qsort comparators)

The standard library's `qsort` sorts *any* element type; it just needs a
comparator taking two `const void *`. The comparator casts back to
`const int *` and returns negative/zero/positive. Two subtleties:

- `(x > y) - (x < y)` instead of `x - y`, which can overflow.
- `compare_ints_desc` is implemented by calling `compare_ints_asc(b, a)` —
  flipping the arguments flips the order.

### `demo_searching` / `demo_sorting`

Run every algorithm on the same input (copied each time with `memcpy` so each
sort starts from identical data) and print the results side by side.

---

## Chapter 10 — File I/O

Files in C are `FILE *` streams from `<stdio.h>`. The lifecycle is always
open → check for NULL → use → **close**. Forgetting `fclose` leaks the OS
handle and may lose buffered output.

### `write_lines_to_file(const char *path, const char *const *lines, size_t count)` → `bool`

Opens in mode `"w"` (create or truncate), writes each string as a line with
`fprintf`, closes. On failure it prints `strerror(errno)` — the OS's
human-readable reason — and returns false.

### `read_file_lines(const char *path)` → `long`

Opens in `"r"` and reads line by line with `fgets` into a fixed buffer.
`fgets` keeps the trailing `'\n'`, so the demo strips it with the idiom
`buffer[strcspn(buffer, "\n")] = '\0'`. Returns the number of lines, or −1 if
the file couldn't be opened.

### `append_line_to_file(const char *path, const char *line)` → `bool`

Mode `"a"` positions every write at end-of-file, preserving existing content.

### `save_students_binary` / `load_students_binary`

Binary I/O with `fwrite`/`fread` in modes `"wb"`/`"rb"`: an array of `Student`
structs is dumped and reloaded **byte-for-byte**. Compact and fast, with an
essential caveat taught alongside: raw struct dumps are **not portable**
across machines with different endianness, padding, or struct layout — real
formats define an explicit byte-level encoding. `load` returns how many
records `fread` actually delivered, so partial files are handled.

### `demo_file_io(void)`

Writes, appends, and reads back a text file; round-trips two students through
a binary file; then deletes both demo files with `remove` so repeated runs
start clean.

---

## Chapter 11 — Bit Manipulation

All functions operate on `unsigned` values — shifting signed negative numbers
is implementation-defined or undefined, so unsigned types are the rule for
bit work.

### The four fundamental single-bit operations

| Function | Expression | Effect |
|---|---|---|
| `set_bit(v, n)` | `v \| (1u << n)` | force bit *n* to 1 |
| `clear_bit(v, n)` | `v & ~(1u << n)` | force bit *n* to 0 |
| `toggle_bit(v, n)` | `v ^ (1u << n)` | flip bit *n* |
| `test_bit(v, n)` | `(v >> n) & 1u` | read bit *n* |

Everything is built from a **mask** `1u << n` combined with OR, AND-with-NOT,
XOR, and shift-and-AND. These patterns are ubiquitous in embedded programming
(device registers), flags fields, and bitmap data structures.

### `count_set_bits(unsigned value)` → `unsigned`

**Brian Kernighan's trick**: `value & (value - 1)` clears the lowest set bit,
so the loop runs once per 1-bit instead of once per bit position.

### `is_power_of_two(unsigned value)` → `bool`

A power of two has exactly one set bit, so `v != 0 && (v & (v - 1)) == 0`.

### `print_binary(unsigned value, unsigned bits)`

Prints the low `bits` bits MSB-first by testing each position from high to
low. Note the loop `for (i = bits; i-- > 0;)` — the safe way to count down
with an unsigned index (an unsigned `i >= 0` is always true).

### `demo_bits(void)`

Builds a value bit by bit with binary output after each step, then shows
shifts as multiplication/division by powers of two (`1u << 4 == 16`) and
masking out the low byte (`0xABCD & 0xFF == 0xCD`).

---

## Chapter 12 — Advanced Topics

### `sum_variadic(int count, ...)` → `long`

A **variadic function** (like `printf`). `<stdarg.h>` provides the tools:
`va_start` initializes the walk, `va_arg(args, int)` fetches each argument,
`va_end` cleans up. C gives variadic functions *no way* to know how many
arguments they received or their types — hence the explicit `count` parameter
(printf infers the same information from its format string).

### `next_id(void)` → `int`

A `static` **local** variable is initialized once and retains its value
across calls, giving the function private persistent state without a global
variable. Discussion point: this is not thread-safe.

### `safe_divide(int numerator, int denominator, int *result)` → `bool`

C has no exceptions, so the idiomatic error pattern is: **return a status,
deliver the value through an out-parameter**. The function refuses division
by zero and the subtle `INT_MIN / -1` case (which overflows on
two's-complement machines) instead of crashing.

### `demo_error_handling(void)`

Exercises `safe_divide`, then demonstrates `errno`: after a failed library
call like `fopen` on a nonexistent path, `errno` holds a code explaining
*why*, and `strerror(errno)` renders it as text.

### `generic_swap(void *a, void *b, size_t size)`

Type-generic programming with `void *`: swaps `size` raw bytes between any
two objects using `memcpy` through a temporary buffer. This is exactly how
`qsort` moves elements without knowing their type. The demo swaps both `int`s
and `double`s with the same function.

### `for_each_int(int *arr, size_t len, void (*fn)(int *, void *), void *context)`

A higher-order iteration helper: applies a callback to every element. The
`context` pointer lets the caller thread arbitrary state through the callback
without global variables — the standard C callback idiom (seen in real APIs
like `qsort_r`, GLib, and most C event loops). Two callbacks demonstrate it:

- **`scale_element`** — multiplies each element by a factor passed via context.
- **`accumulate_element`** — sums elements into a `long` passed via context.

### `demo_variadic` / `demo_static_and_macros` / `demo_generic_programming`

Drivers that exercise the above, including `MAX`/`MIN`/`ARRAY_LEN` macro
behavior.

---

## `main(int argc, char *argv[])`

The program entry point. Demonstrates **command-line arguments**: `argc` is
the argument count, `argv[0]` is the program name, and `argv[1]`…
`argv[argc-1]` are user-supplied arguments (try `./main hello world`). It then
runs every chapter in order and returns `EXIT_SUCCESS` (0), the conventional
"all fine" exit status checked by shells and build tools.

---

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Beginner | 1–3 | FizzBuzz, temperature converter, re-implement `my_strstr` |
| Intermediate | 4–7 | Dynamic matrix (heap-allocated rows), string vector, `vec_pop`/`vec_insert` |
| Advanced | 8–10 | Doubly linked list, BST delete, hash table resizing, CSV parser |
| Expert | 11–12 | Bit-set data structure, generic `map`/`filter`, error-code enum design |
