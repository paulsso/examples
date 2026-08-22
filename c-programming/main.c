/*
 * ============================================================================
 *  main.c — A Course in C Programming: From Beginner to Advanced
 * ============================================================================
 *
 *  This file is the backbone of a C programming course. It is organized as
 *  a sequence of "chapters", each demonstrating a family of operations that
 *  every C programmer should master. Every function is documented here with
 *  a comment block, and explained in depth in DOCUMENTATION.md.
 *
 *  Chapters:
 *    1.  Fundamentals        — types, operators, control flow, loops
 *    2.  Functions           — parameters, return values, recursion
 *    3.  Arrays & Strings    — iteration, aggregation, manual string ops
 *    4.  Matrices            — 2D arrays, addition, multiplication
 *    5.  Pointers            — dereferencing, arithmetic, function pointers
 *    6.  Structs & Enums     — user-defined types, unions, typedef
 *    7.  Dynamic Memory      — malloc/calloc/realloc/free, growable arrays
 *    8.  Data Structures     — linked list, stack, queue, BST, hash table
 *    9.  Algorithms          — searching and sorting
 *    10. File I/O            — text and binary files
 *    11. Bit Manipulation    — masks, shifts, common bit tricks
 *    12. Advanced Topics     — variadic functions, macros, statics,
 *                              error handling, generic (void*) programming
 *
 *  Build:  make          (produces the `main` binary)
 *  Run:    ./main        (or `make run`)
 * ============================================================================
 */

#include <stdio.h>    /* printf, fopen, fgets, ...          */
#include <stdlib.h>   /* malloc, free, qsort, exit, ...     */
#include <string.h>   /* strlen, strcpy, memcpy, ...        */
#include <stdbool.h>  /* bool, true, false                  */
#include <stdarg.h>   /* variadic functions (va_list, ...)  */
#include <stddef.h>   /* size_t, NULL                       */
#include <limits.h>   /* INT_MAX, INT_MIN, CHAR_BIT         */
#include <ctype.h>    /* toupper, tolower, isalpha          */
#include <math.h>     /* sqrt (link with -lm)               */
#include <errno.h>    /* errno, strerror                    */

/* ---------------------------------------------------------------------------
 * Preprocessor macros (explained in Chapter 12, used throughout).
 * ------------------------------------------------------------------------- */

/* Number of elements in a statically-sized array. Only valid on true arrays,
 * never on pointers (a pointer's size is not related to what it points at). */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Classic function-like macros. Note the parentheses around every argument
 * and around the whole expansion: they prevent operator-precedence bugs. */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Prints a chapter banner so the program output is easy to follow. */
#define CHAPTER(title)                                                \
    printf("\n=============================================\n"       \
           " %s\n"                                                    \
           "=============================================\n", title)

/* ===========================================================================
 * CHAPTER 1 — FUNDAMENTALS
 * ===========================================================================
 */

/*
 * demo_data_types
 * ---------------
 * Shows the fundamental scalar types of C, how to declare and initialize
 * them, their sizes on the current platform (via sizeof) and the printf
 * conversion specifier used for each.
 */
static void demo_data_types(void)
{
    char letter = 'A';                 /* single character (really a small int) */
    int count = -42;                   /* signed integer                        */
    unsigned int positive = 42u;       /* unsigned: only >= 0 values            */
    long big = 1234567890L;            /* at least 32 bits, usually 64          */
    long long bigger = 9876543210LL;   /* at least 64 bits                      */
    float ratio = 3.14f;               /* single-precision floating point       */
    double precise = 2.718281828459;   /* double-precision floating point       */
    bool flag = true;                  /* from <stdbool.h> (C99)                */

    printf("char      %c   (size %zu byte)\n", letter, sizeof letter);
    printf("int       %d  (size %zu bytes, range %d..%d)\n",
           count, sizeof count, INT_MIN, INT_MAX);
    printf("unsigned  %u   (size %zu bytes)\n", positive, sizeof positive);
    printf("long      %ld (size %zu bytes)\n", big, sizeof big);
    printf("long long %lld (size %zu bytes)\n", bigger, sizeof bigger);
    printf("float     %f (size %zu bytes)\n", (double)ratio, sizeof ratio);
    printf("double    %.9f (size %zu bytes)\n", precise, sizeof precise);
    printf("bool      %s (size %zu byte)\n", flag ? "true" : "false", sizeof flag);
}

/*
 * demo_operators
 * --------------
 * Demonstrates arithmetic, integer division vs. floating-point division,
 * the modulo operator, comparison and logical operators, and the difference
 * between prefix and postfix increment.
 */
static void demo_operators(void)
{
    int a = 17, b = 5;

    printf("a = %d, b = %d\n", a, b);
    printf("a + b = %d, a - b = %d, a * b = %d\n", a + b, a - b, a * b);
    printf("a / b = %d      (integer division truncates)\n", a / b);
    printf("a %% b = %d      (remainder / modulo)\n", a % b);
    printf("(double)a / b = %.2f (cast forces real division)\n", (double)a / b);

    printf("a > b: %d, a == b: %d, a != b: %d\n", a > b, a == b, a != b);
    printf("(a > 0 && b > 0): %d, (a < 0 || b > 0): %d, !(a > b): %d\n",
           a > 0 && b > 0, a < 0 || b > 0, !(a > b));

    /* Postfix x++ yields the OLD value; prefix ++x yields the NEW value.
     * Note: never mix x++ and x in one expression (e.g. printf(.., x++, x)) —
     * the evaluation order is unsequenced and the behavior undefined. */
    int x = 5;
    int postfix_result = x++;
    printf("x = 5; x++ evaluates to %d, then x is %d\n", postfix_result, x);
    x = 5;
    int prefix_result = ++x;
    printf("x = 5; ++x evaluates to %d, then x is %d\n", prefix_result, x);

    /* Compound assignment operators. */
    int acc = 10;
    acc += 5;  printf("acc += 5  -> %d\n", acc);
    acc -= 3;  printf("acc -= 3  -> %d\n", acc);
    acc *= 2;  printf("acc *= 2  -> %d\n", acc);
    acc /= 4;  printf("acc /= 4  -> %d\n", acc);

    /* Ternary conditional operator: condition ? if_true : if_false. */
    int bigger = (a > b) ? a : b;
    printf("ternary: bigger of %d and %d is %d\n", a, b, bigger);
}

/*
 * classify_number
 * ---------------
 * Returns a human-readable classification of an integer. Demonstrates
 * if / else-if / else chains as an expression of decision logic.
 */
static const char *classify_number(int n)
{
    if (n < 0) {
        return "negative";
    } else if (n == 0) {
        return "zero";
    } else if (n % 2 == 0) {
        return "positive even";
    } else {
        return "positive odd";
    }
}

/*
 * weekday_name
 * ------------
 * Maps a day index (1..7) to its name using a switch statement.
 * Demonstrates case labels, break, and a default fallback.
 */
static const char *weekday_name(int day)
{
    switch (day) {
    case 1: return "Monday";
    case 2: return "Tuesday";
    case 3: return "Wednesday";
    case 4: return "Thursday";
    case 5: return "Friday";
    case 6: return "Saturday";
    case 7: return "Sunday";
    default: return "invalid day";
    }
}

/*
 * demo_control_flow
 * -----------------
 * Exercises classify_number and weekday_name to show branching in action.
 */
static void demo_control_flow(void)
{
    int samples[] = { -7, 0, 4, 9 };
    for (size_t i = 0; i < ARRAY_LEN(samples); i++) {
        printf("%3d is %s\n", samples[i], classify_number(samples[i]));
    }
    printf("day 3 is %s, day 9 is %s\n", weekday_name(3), weekday_name(9));
}

/*
 * demo_loops
 * ----------
 * Shows the three loop constructs (for, while, do-while) plus the loop
 * control statements break and continue.
 */
static void demo_loops(void)
{
    printf("for      : ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", i);
    }
    printf("\n");

    printf("while    : ");
    int n = 5;
    while (n > 0) {
        printf("%d ", n);
        n--;
    }
    printf("\n");

    /* do-while always runs the body at least once. */
    printf("do-while : ");
    int m = 0;
    do {
        printf("%d ", m);
        m++;
    } while (m < 3);
    printf("\n");

    printf("break    : ");
    for (int i = 0; i < 100; i++) {
        if (i == 4) break;          /* exit the loop entirely */
        printf("%d ", i);
    }
    printf("\n");

    printf("continue : ");
    for (int i = 0; i < 10; i++) {
        if (i % 2 == 0) continue;   /* skip to the next iteration */
        printf("%d ", i);
    }
    printf("(odd numbers only)\n");

    /* Nested loops: a small multiplication table. */
    printf("nested loops (3x3 multiplication table):\n");
    for (int row = 1; row <= 3; row++) {
        for (int col = 1; col <= 3; col++) {
            printf("%4d", row * col);
        }
        printf("\n");
    }
}

/* ===========================================================================
 * CHAPTER 2 — FUNCTIONS
 * ===========================================================================
 */

/*
 * add
 * ---
 * The simplest possible function: takes two ints by value, returns their sum.
 * Arguments in C are always passed by value — the function gets copies.
 */
static int add(int a, int b)
{
    return a + b;
}

/*
 * factorial_recursive
 * -------------------
 * Computes n! recursively: n! = n * (n-1)!, with base case 0! = 1.
 * A textbook example of recursion. Uses long long because factorials
 * grow extremely fast (13! already overflows a 32-bit int).
 */
static long long factorial_recursive(int n)
{
    if (n <= 1) {
        return 1;               /* base case stops the recursion */
    }
    return (long long)n * factorial_recursive(n - 1);
}

/*
 * factorial_iterative
 * -------------------
 * The same computation with a loop. Iteration avoids the function-call
 * overhead and stack growth of recursion; comparing the two versions is
 * a classic teaching exercise.
 */
static long long factorial_iterative(int n)
{
    long long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

/*
 * fibonacci
 * ---------
 * Returns the n-th Fibonacci number iteratively (0, 1, 1, 2, 3, 5, ...).
 * The naive recursive version is exponential; this loop is O(n).
 */
static long long fibonacci(int n)
{
    long long prev = 0, curr = 1;
    if (n == 0) return 0;
    for (int i = 2; i <= n; i++) {
        long long next = prev + curr;
        prev = curr;
        curr = next;
    }
    return curr;
}

/*
 * gcd
 * ---
 * Greatest common divisor via Euclid's algorithm: gcd(a, b) = gcd(b, a mod b)
 * until b becomes 0. One of the oldest algorithms known, and a neat example
 * of a while loop replacing recursion.
 */
static int gcd(int a, int b)
{
    while (b != 0) {
        int remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

/*
 * int_power
 * ---------
 * Raises base to a non-negative exponent using repeated multiplication.
 * Illustrates accumulating a result in a loop.
 */
static long long int_power(int base, unsigned int exponent)
{
    long long result = 1;
    for (unsigned int i = 0; i < exponent; i++) {
        result *= base;
    }
    return result;
}

/*
 * is_prime
 * --------
 * Trial division primality test. Only checks divisors up to sqrt(n),
 * because a composite number must have a factor no larger than its root.
 */
static bool is_prime(int n)
{
    if (n < 2) return false;
    if (n < 4) return true;         /* 2 and 3 are prime */
    if (n % 2 == 0) return false;
    for (int d = 3; (long long)d * d <= n; d += 2) {
        if (n % d == 0) return false;
    }
    return true;
}

/*
 * demo_functions
 * --------------
 * Drives the functions above with sample inputs.
 */
static void demo_functions(void)
{
    printf("add(3, 4)                = %d\n", add(3, 4));
    printf("factorial_recursive(10)  = %lld\n", factorial_recursive(10));
    printf("factorial_iterative(10)  = %lld\n", factorial_iterative(10));
    printf("fibonacci(20)            = %lld\n", fibonacci(20));
    printf("gcd(48, 36)              = %d\n", gcd(48, 36));
    printf("int_power(2, 16)         = %lld\n", int_power(2, 16));
    printf("primes below 30          : ");
    for (int i = 2; i < 30; i++) {
        if (is_prime(i)) printf("%d ", i);
    }
    printf("\n");
}

/* ===========================================================================
 * CHAPTER 3 — ARRAYS & STRINGS
 * ===========================================================================
 */

/*
 * array_sum / array_max / array_min / array_average
 * -------------------------------------------------
 * Fundamental aggregation over an array. Arrays "decay" to pointers when
 * passed to functions, so the length must be passed separately — C arrays
 * do not carry their own size.
 */
static long array_sum(const int *arr, size_t len)
{
    long total = 0;
    for (size_t i = 0; i < len; i++) {
        total += arr[i];
    }
    return total;
}

static int array_max(const int *arr, size_t len)
{
    int best = arr[0];
    for (size_t i = 1; i < len; i++) {
        if (arr[i] > best) best = arr[i];
    }
    return best;
}

static int array_min(const int *arr, size_t len)
{
    int best = arr[0];
    for (size_t i = 1; i < len; i++) {
        if (arr[i] < best) best = arr[i];
    }
    return best;
}

static double array_average(const int *arr, size_t len)
{
    return (double)array_sum(arr, len) / (double)len;
}

/*
 * array_reverse
 * -------------
 * Reverses an array in place using the two-pointer technique: swap the
 * outermost pair, then move both indices inward until they meet.
 */
static void array_reverse(int *arr, size_t len)
{
    for (size_t i = 0, j = len - 1; i < j; i++, j--) {
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/*
 * print_int_array
 * ---------------
 * Small utility used throughout the program to display arrays.
 */
static void print_int_array(const int *arr, size_t len)
{
    printf("[");
    for (size_t i = 0; i < len; i++) {
        printf("%d%s", arr[i], (i + 1 < len) ? ", " : "");
    }
    printf("]");
}

/*
 * my_strlen
 * ---------
 * Re-implementation of strlen: counts characters until the '\0' terminator.
 * Understanding that C strings are just char arrays ending in a zero byte
 * is one of the most important ideas in the language.
 */
static size_t my_strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/*
 * my_strcpy
 * ---------
 * Re-implementation of strcpy: copies src into dst including the '\0'.
 * The caller must guarantee that dst is large enough — C never checks.
 */
static char *my_strcpy(char *dst, const char *src)
{
    char *start = dst;
    while ((*dst++ = *src++) != '\0') {
        /* the copy happens in the loop condition itself */
    }
    return start;
}

/*
 * my_strcmp
 * ---------
 * Re-implementation of strcmp: lexicographic comparison. Returns a
 * negative value, zero, or a positive value — the same contract used by
 * qsort and bsearch comparators.
 */
static int my_strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/*
 * my_strcat
 * ---------
 * Re-implementation of strcat: walks to the end of dst, then copies src.
 */
static char *my_strcat(char *dst, const char *src)
{
    char *start = dst;
    while (*dst != '\0') {
        dst++;
    }
    my_strcpy(dst, src);
    return start;
}

/*
 * string_reverse
 * --------------
 * Reverses a string in place — the same two-pointer swap as array_reverse,
 * applied to chars.
 */
static void string_reverse(char *s)
{
    size_t len = my_strlen(s);
    for (size_t i = 0, j = len - 1; len > 0 && i < j; i++, j--) {
        char tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
    }
}

/*
 * is_palindrome
 * -------------
 * Checks whether a string reads the same forwards and backwards,
 * ignoring case and non-alphanumeric characters ("Racecar!" -> true).
 */
static bool is_palindrome(const char *s)
{
    size_t i = 0, j = my_strlen(s);
    if (j == 0) return true;
    j--;
    while (i < j) {
        while (i < j && !isalnum((unsigned char)s[i])) i++;
        while (i < j && !isalnum((unsigned char)s[j])) j--;
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)s[j])) {
            return false;
        }
        i++;
        if (j > 0) j--;
    }
    return true;
}

/*
 * count_vowels
 * ------------
 * Counts vowels in a string. Demonstrates iterating a string with a
 * pointer and using strchr for set membership.
 */
static size_t count_vowels(const char *s)
{
    size_t count = 0;
    for (const char *p = s; *p != '\0'; p++) {
        if (strchr("aeiouAEIOU", *p) != NULL) {
            count++;
        }
    }
    return count;
}

/*
 * string_to_upper
 * ---------------
 * Uppercases a string in place using toupper from <ctype.h>. The cast to
 * unsigned char avoids undefined behavior for negative char values.
 */
static void string_to_upper(char *s)
{
    for (; *s != '\0'; s++) {
        *s = (char)toupper((unsigned char)*s);
    }
}

/*
 * count_words
 * -----------
 * Counts whitespace-separated words by detecting transitions from
 * "in whitespace" to "in a word" — a miniature state machine.
 */
static size_t count_words(const char *s)
{
    size_t count = 0;
    bool in_word = false;
    for (; *s != '\0'; s++) {
        if (isspace((unsigned char)*s)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

static void demo_arrays(void)
{
    int numbers[] = { 8, 3, 5, 1, 9, 2 };
    size_t len = ARRAY_LEN(numbers);

    printf("array          : ");
    print_int_array(numbers, len);
    printf("\n");
    printf("sum = %ld, max = %d, min = %d, average = %.2f\n",
           array_sum(numbers, len), array_max(numbers, len),
           array_min(numbers, len), array_average(numbers, len));

    array_reverse(numbers, len);
    printf("reversed       : ");
    print_int_array(numbers, len);
    printf("\n");
}

static void demo_strings(void)
{
    char buffer[64];

    printf("my_strlen(\"hello world\")   = %zu\n", my_strlen("hello world"));

    my_strcpy(buffer, "hello");
    my_strcat(buffer, ", world");
    printf("copy + concat              = \"%s\"\n", buffer);

    printf("my_strcmp(\"apple\",\"banana\") = %d (negative: apple < banana)\n",
           my_strcmp("apple", "banana"));

    string_reverse(buffer);
    printf("reversed                   = \"%s\"\n", buffer);

    printf("is_palindrome(\"Racecar!\")  = %s\n",
           is_palindrome("Racecar!") ? "true" : "false");
    printf("is_palindrome(\"hello\")     = %s\n",
           is_palindrome("hello") ? "true" : "false");

    printf("count_vowels(\"programming\") = %zu\n", count_vowels("programming"));
    printf("count_words(\"the quick brown fox\") = %zu\n",
           count_words("the quick brown fox"));

    char shout[] = "make it loud";
    string_to_upper(shout);
    printf("string_to_upper            = \"%s\"\n", shout);
}

/* ===========================================================================
 * CHAPTER 4 — MATRICES (2D ARRAYS)
 * ===========================================================================
 */

#define MAT_N 3   /* fixed matrix dimension for the demos */

/*
 * matrix_print
 * ------------
 * Prints a 3x3 matrix. A 2D array parameter must declare all dimensions
 * except the first, because the compiler needs them to compute element
 * addresses (row-major layout: element [r][c] lives at r*cols + c).
 *
 * Note: these parameters are not declared `const int [N][N]` because,
 * before C23, C does not allow passing a non-const 2D array to such a
 * parameter without a warning — a well-known quirk of the language.
 */
static void matrix_print(int m[MAT_N][MAT_N])
{
    for (int r = 0; r < MAT_N; r++) {
        printf("    ");
        for (int c = 0; c < MAT_N; c++) {
            printf("%4d", m[r][c]);
        }
        printf("\n");
    }
}

/*
 * matrix_add
 * ----------
 * Element-wise addition: out[r][c] = a[r][c] + b[r][c].
 */
static void matrix_add(int a[MAT_N][MAT_N], int b[MAT_N][MAT_N],
                       int out[MAT_N][MAT_N])
{
    for (int r = 0; r < MAT_N; r++) {
        for (int c = 0; c < MAT_N; c++) {
            out[r][c] = a[r][c] + b[r][c];
        }
    }
}

/*
 * matrix_multiply
 * ---------------
 * Standard O(n^3) matrix multiplication: each output cell is the dot
 * product of a row of `a` with a column of `b`. The triple nested loop
 * is a rite of passage for every C student.
 */
static void matrix_multiply(int a[MAT_N][MAT_N], int b[MAT_N][MAT_N],
                            int out[MAT_N][MAT_N])
{
    for (int r = 0; r < MAT_N; r++) {
        for (int c = 0; c < MAT_N; c++) {
            int dot = 0;
            for (int k = 0; k < MAT_N; k++) {
                dot += a[r][k] * b[k][c];
            }
            out[r][c] = dot;
        }
    }
}

/*
 * matrix_transpose
 * ----------------
 * Flips a matrix over its diagonal: out[c][r] = in[r][c].
 */
static void matrix_transpose(int in[MAT_N][MAT_N], int out[MAT_N][MAT_N])
{
    for (int r = 0; r < MAT_N; r++) {
        for (int c = 0; c < MAT_N; c++) {
            out[c][r] = in[r][c];
        }
    }
}

static void demo_matrices(void)
{
    int a[MAT_N][MAT_N] = { {1, 2, 3}, {4, 5, 6}, {7, 8, 9} };
    int identity[MAT_N][MAT_N] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    int result[MAT_N][MAT_N];

    printf("matrix a:\n");
    matrix_print(a);

    matrix_add(a, identity, result);
    printf("a + identity:\n");
    matrix_print(result);

    matrix_multiply(a, identity, result);
    printf("a * identity (should equal a):\n");
    matrix_print(result);

    matrix_transpose(a, result);
    printf("transpose of a:\n");
    matrix_print(result);
}

/* ===========================================================================
 * CHAPTER 5 — POINTERS
 * ===========================================================================
 */

/*
 * swap
 * ----
 * THE canonical pointer example. Because C passes arguments by value,
 * a function cannot modify its caller's variables — unless the caller
 * passes their addresses. swap dereferences the pointers to exchange
 * the pointed-at values.
 */
static void swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * find_max_ptr
 * ------------
 * Returns a POINTER to the largest element rather than its value.
 * Returning a pointer lets the caller both read and modify the element.
 */
static int *find_max_ptr(int *arr, size_t len)
{
    int *best = &arr[0];
    for (size_t i = 1; i < len; i++) {
        if (arr[i] > *best) {
            best = &arr[i];
        }
    }
    return best;
}

/*
 * demo_pointers
 * -------------
 * Shows the address-of (&) and dereference (*) operators, pointer
 * arithmetic on arrays, NULL, and pointers to pointers.
 */
static void demo_pointers(void)
{
    int value = 10;
    int *ptr = &value;              /* ptr holds the ADDRESS of value */

    printf("value = %d, &value = %p\n", value, (void *)&value);
    printf("ptr   = %p, *ptr = %d\n", (void *)ptr, *ptr);

    *ptr = 99;                      /* writing through the pointer */
    printf("after *ptr = 99, value = %d\n", value);

    int x = 1, y = 2;
    swap(&x, &y);
    printf("after swap: x = %d, y = %d\n", x, y);

    /* Pointer arithmetic: arr[i] is literally *(arr + i). */
    int arr[] = { 10, 20, 30, 40 };
    int *p = arr;                   /* arrays decay to a pointer to [0] */
    printf("pointer walk : ");
    for (size_t i = 0; i < ARRAY_LEN(arr); i++) {
        printf("%d ", *(p + i));
    }
    printf("\n");

    int *max = find_max_ptr(arr, ARRAY_LEN(arr));
    printf("max element is %d at index %td\n", *max, max - arr);
    *max = 0;                       /* modify the array through the pointer */
    printf("after zeroing max: ");
    print_int_array(arr, ARRAY_LEN(arr));
    printf("\n");

    /* Pointer to pointer: pp -> ptr -> value. */
    int **pp = &ptr;
    printf("pointer to pointer: **pp = %d\n", **pp);

    /* NULL means "points at nothing"; always check before dereferencing. */
    int *nothing = NULL;
    printf("NULL pointer is %s\n", (nothing == NULL) ? "NULL (do not dereference!)" : "set");
}

/*
 * Function pointers
 * -----------------
 * A function pointer stores the address of a function, enabling callbacks
 * and table-driven designs. `apply_operation` takes the operation to
 * perform as a parameter — the essence of higher-order programming in C.
 */
static int op_add(int a, int b) { return a + b; }
static int op_sub(int a, int b) { return a - b; }
static int op_mul(int a, int b) { return a * b; }

/*
 * apply_operation
 * ---------------
 * Invokes whatever binary int function it is given. The parameter type
 * `int (*op)(int, int)` reads as "pointer to function taking two ints
 * and returning int".
 */
static int apply_operation(int a, int b, int (*op)(int, int))
{
    return op(a, b);
}

static void demo_function_pointers(void)
{
    printf("apply_operation(6, 7, op_add) = %d\n", apply_operation(6, 7, op_add));
    printf("apply_operation(6, 7, op_sub) = %d\n", apply_operation(6, 7, op_sub));
    printf("apply_operation(6, 7, op_mul) = %d\n", apply_operation(6, 7, op_mul));

    /* A dispatch table: an array of function pointers. */
    struct {
        const char *name;
        int (*fn)(int, int);
    } ops[] = {
        { "add", op_add },
        { "sub", op_sub },
        { "mul", op_mul },
    };
    for (size_t i = 0; i < ARRAY_LEN(ops); i++) {
        printf("table dispatch %s(9, 4) = %d\n", ops[i].name, ops[i].fn(9, 4));
    }
}

/* ===========================================================================
 * CHAPTER 6 — STRUCTS, ENUMS, UNIONS
 * ===========================================================================
 */

/*
 * struct Point
 * ------------
 * A struct groups related data into a single type. Points are a natural
 * first example: two doubles with geometric meaning.
 */
typedef struct {
    double x;
    double y;
} Point;

/*
 * point_distance
 * --------------
 * Euclidean distance between two points. Structs are passed by value
 * here (copies), which is fine for small types.
 */
static double point_distance(Point a, Point b)
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

/*
 * struct Student + student_print
 * ------------------------------
 * A more realistic record type mixing a string field, an int, and a
 * double. student_print takes a POINTER to avoid copying the struct and
 * uses the arrow operator (->) to access members through the pointer.
 */
typedef struct {
    char name[32];
    int id;
    double gpa;
} Student;

static void student_print(const Student *s)
{
    printf("Student { name: %-8s id: %d, gpa: %.2f }\n", s->name, s->id, s->gpa);
}

/*
 * enum Color + color_name
 * -----------------------
 * Enums give names to integer constants. color_name maps them back to
 * strings for display.
 */
typedef enum {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
} Color;

static const char *color_name(Color c)
{
    switch (c) {
    case COLOR_RED:   return "red";
    case COLOR_GREEN: return "green";
    case COLOR_BLUE:  return "blue";
    default:          return "unknown";
    }
}

/*
 * union Value
 * -----------
 * A union overlays several types on the SAME memory. Only the most
 * recently written member is valid. Unions save space and appear in
 * variant/tagged-value designs; the tag (enum) records which member
 * is currently active.
 */
typedef enum { VALUE_INT, VALUE_DOUBLE, VALUE_STRING } ValueTag;

typedef struct {
    ValueTag tag;
    union {
        int i;
        double d;
        const char *s;
    } as;
} TaggedValue;

static void tagged_value_print(const TaggedValue *v)
{
    switch (v->tag) {
    case VALUE_INT:    printf("int %d\n", v->as.i); break;
    case VALUE_DOUBLE: printf("double %.3f\n", v->as.d); break;
    case VALUE_STRING: printf("string \"%s\"\n", v->as.s); break;
    }
}

static void demo_structs(void)
{
    Point origin = { 0.0, 0.0 };
    Point p = { 3.0, 4.0 };
    printf("distance from (0,0) to (3,4) = %.1f\n", point_distance(origin, p));

    Student alice = { "Alice", 1001, 3.85 };
    Student bob;
    strcpy(bob.name, "Bob");        /* strings inside structs need strcpy */
    bob.id = 1002;
    bob.gpa = 3.42;
    student_print(&alice);
    student_print(&bob);

    Color c = COLOR_GREEN;
    printf("enum Color value %d is \"%s\"\n", (int)c, color_name(c));

    TaggedValue values[] = {
        { VALUE_INT,    { .i = 42 } },
        { VALUE_DOUBLE, { .d = 3.14159 } },
        { VALUE_STRING, { .s = "hello" } },
    };
    printf("union sizes: int=%zu double=%zu ptr=%zu, union itself=%zu (max of members)\n",
           sizeof(int), sizeof(double), sizeof(const char *), sizeof(values[0].as));
    for (size_t i = 0; i < ARRAY_LEN(values); i++) {
        printf("tagged value %zu: ", i);
        tagged_value_print(&values[i]);
    }
}

/* ===========================================================================
 * CHAPTER 7 — DYNAMIC MEMORY
 * ===========================================================================
 */

/*
 * make_range
 * ----------
 * Allocates an int array on the heap with malloc and fills it with
 * 0..n-1. The caller owns the memory and must free() it. Every malloc
 * result must be checked for NULL (allocation can fail).
 */
static int *make_range(size_t n)
{
    int *arr = malloc(n * sizeof *arr);
    if (arr == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        arr[i] = (int)i;
    }
    return arr;
}

/*
 * my_strdup
 * ---------
 * Duplicates a string on the heap (like POSIX strdup): measure, allocate
 * length+1 bytes (the +1 is for the terminator!), copy.
 */
static char *my_strdup(const char *s)
{
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

/*
 * IntVector — a growable dynamic array (the pattern behind C++'s vector).
 * -----------------------------------------------------------------------
 * Keeps three fields: the heap buffer, how many slots are used (size),
 * and how many are allocated (capacity). When full it doubles capacity
 * with realloc, giving amortized O(1) appends.
 */
typedef struct {
    int *data;
    size_t size;
    size_t capacity;
} IntVector;

/* vec_init: start empty; the first push allocates. */
static void vec_init(IntVector *v)
{
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

/* vec_push: append one element, growing the buffer if needed.
 * Returns false if realloc fails (original buffer stays valid). */
static bool vec_push(IntVector *v, int value)
{
    if (v->size == v->capacity) {
        size_t new_capacity = (v->capacity == 0) ? 4 : v->capacity * 2;
        int *new_data = realloc(v->data, new_capacity * sizeof *new_data);
        if (new_data == NULL) {
            return false;
        }
        v->data = new_data;
        v->capacity = new_capacity;
    }
    v->data[v->size++] = value;
    return true;
}

/* vec_get: bounds-checked read. */
static int vec_get(const IntVector *v, size_t index)
{
    if (index >= v->size) {
        fprintf(stderr, "vec_get: index %zu out of bounds (size %zu)\n",
                index, v->size);
        exit(EXIT_FAILURE);
    }
    return v->data[index];
}

/* vec_free: release the buffer and reset, preventing use-after-free. */
static void vec_free(IntVector *v)
{
    free(v->data);
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

static void demo_dynamic_memory(void)
{
    /* malloc + free */
    int *range = make_range(5);
    if (range != NULL) {
        printf("malloc'd range: ");
        print_int_array(range, 5);
        printf("\n");
        free(range);                /* every malloc needs exactly one free */
    }

    /* calloc zero-initializes, unlike malloc. */
    int *zeros = calloc(4, sizeof *zeros);
    if (zeros != NULL) {
        printf("calloc'd zeros: ");
        print_int_array(zeros, 4);
        printf("\n");
        free(zeros);
    }

    /* Heap-allocated string copy. */
    char *copy = my_strdup("heap string");
    if (copy != NULL) {
        printf("my_strdup      : \"%s\"\n", copy);
        free(copy);
    }

    /* Growable vector exercising realloc. */
    IntVector v;
    vec_init(&v);
    for (int i = 1; i <= 10; i++) {
        vec_push(&v, i * i);
    }
    printf("vector of squares (size %zu, capacity %zu): ", v.size, v.capacity);
    print_int_array(v.data, v.size);
    printf("\n");
    printf("vec_get(&v, 3) = %d\n", vec_get(&v, 3));
    vec_free(&v);
}

/* ===========================================================================
 * CHAPTER 8 — DATA STRUCTURES
 * ===========================================================================
 */

/* ---------------------------- Singly linked list ------------------------- */

/*
 * A node owns a value and a pointer to the next node; NULL marks the end.
 * The list itself is just a pointer to the first node (the head).
 */
typedef struct ListNode {
    int value;
    struct ListNode *next;
} ListNode;

/*
 * list_push_front
 * ---------------
 * O(1) insertion at the head. Takes ListNode** so it can modify the
 * caller's head pointer — a great exercise in double indirection.
 */
static bool list_push_front(ListNode **head, int value)
{
    ListNode *node = malloc(sizeof *node);
    if (node == NULL) return false;
    node->value = value;
    node->next = *head;
    *head = node;
    return true;
}

/*
 * list_push_back
 * --------------
 * O(n) insertion at the tail: walk to the last node, then link.
 */
static bool list_push_back(ListNode **head, int value)
{
    ListNode *node = malloc(sizeof *node);
    if (node == NULL) return false;
    node->value = value;
    node->next = NULL;

    if (*head == NULL) {
        *head = node;
    } else {
        ListNode *tail = *head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = node;
    }
    return true;
}

/*
 * list_find
 * ---------
 * Linear scan; returns the first node holding `value`, or NULL.
 */
static ListNode *list_find(ListNode *head, int value)
{
    for (ListNode *cur = head; cur != NULL; cur = cur->next) {
        if (cur->value == value) return cur;
    }
    return NULL;
}

/*
 * list_delete
 * -----------
 * Removes the first node holding `value`. Uses a pointer-to-pointer
 * cursor so head removal and middle removal are the same code path.
 * Returns true if a node was removed.
 */
static bool list_delete(ListNode **head, int value)
{
    for (ListNode **cur = head; *cur != NULL; cur = &(*cur)->next) {
        if ((*cur)->value == value) {
            ListNode *doomed = *cur;
            *cur = doomed->next;
            free(doomed);
            return true;
        }
    }
    return false;
}

/*
 * list_reverse
 * ------------
 * Reverses the list in place by re-pointing each node's next at its
 * predecessor. A classic interview question. O(n) time, O(1) space.
 */
static void list_reverse(ListNode **head)
{
    ListNode *prev = NULL;
    ListNode *cur = *head;
    while (cur != NULL) {
        ListNode *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    *head = prev;
}

/*
 * list_length / list_print / list_free
 * ------------------------------------
 * Standard traversal utilities. list_free must grab `next` BEFORE
 * freeing the current node — reading freed memory is undefined behavior.
 */
static size_t list_length(const ListNode *head)
{
    size_t n = 0;
    for (; head != NULL; head = head->next) n++;
    return n;
}

static void list_print(const ListNode *head)
{
    for (; head != NULL; head = head->next) {
        printf("%d -> ", head->value);
    }
    printf("NULL");
}

static void list_free(ListNode **head)
{
    ListNode *cur = *head;
    while (cur != NULL) {
        ListNode *next = cur->next;
        free(cur);
        cur = next;
    }
    *head = NULL;
}

static void demo_linked_list(void)
{
    ListNode *list = NULL;

    list_push_back(&list, 10);
    list_push_back(&list, 20);
    list_push_back(&list, 30);
    list_push_front(&list, 5);

    printf("list           : ");
    list_print(list);
    printf("  (length %zu)\n", list_length(list));

    printf("find 20        : %s\n", list_find(list, 20) ? "found" : "not found");
    list_delete(&list, 20);
    printf("after delete 20: ");
    list_print(list);
    printf("\n");

    list_reverse(&list);
    printf("reversed       : ");
    list_print(list);
    printf("\n");

    list_free(&list);
    printf("after free     : ");
    list_print(list);
    printf("\n");
}

/* ------------------------------- Stack (LIFO) ---------------------------- */

/*
 * A fixed-capacity stack backed by an array. `top` is the index of the
 * next free slot (so top == 0 means empty). Push and pop are O(1).
 */
#define STACK_CAPACITY 16

typedef struct {
    int items[STACK_CAPACITY];
    size_t top;
} Stack;

static void stack_init(Stack *s)          { s->top = 0; }
static bool stack_is_empty(const Stack *s){ return s->top == 0; }
static bool stack_is_full(const Stack *s) { return s->top == STACK_CAPACITY; }

/* stack_push: returns false on overflow instead of corrupting memory. */
static bool stack_push(Stack *s, int value)
{
    if (stack_is_full(s)) return false;
    s->items[s->top++] = value;
    return true;
}

/* stack_pop: writes the removed value through `out`; false on underflow. */
static bool stack_pop(Stack *s, int *out)
{
    if (stack_is_empty(s)) return false;
    *out = s->items[--s->top];
    return true;
}

/* stack_peek: reads the top without removing it. */
static bool stack_peek(const Stack *s, int *out)
{
    if (stack_is_empty(s)) return false;
    *out = s->items[s->top - 1];
    return true;
}

static void demo_stack(void)
{
    Stack s;
    stack_init(&s);

    for (int i = 1; i <= 4; i++) {
        stack_push(&s, i * 11);
        printf("push %d  ", i * 11);
    }
    printf("\n");

    int value = 0;
    if (stack_peek(&s, &value)) {
        printf("peek -> %d (stays on the stack)\n", value);
    }

    printf("pops in LIFO order: ");
    while (stack_pop(&s, &value)) {
        printf("%d ", value);
    }
    printf("\n");
}

/* ------------------------------- Queue (FIFO) ----------------------------- */

/*
 * A circular-buffer queue: `head` is where we dequeue, and elements are
 * enqueued at (head + size) % capacity. Wrapping indices with modulo lets
 * the buffer be reused without shifting elements. O(1) both ends.
 */
#define QUEUE_CAPACITY 8

typedef struct {
    int items[QUEUE_CAPACITY];
    size_t head;
    size_t size;
} Queue;

static void queue_init(Queue *q)           { q->head = 0; q->size = 0; }
static bool queue_is_empty(const Queue *q) { return q->size == 0; }
static bool queue_is_full(const Queue *q)  { return q->size == QUEUE_CAPACITY; }

/* queue_enqueue: add at the back; false when full. */
static bool queue_enqueue(Queue *q, int value)
{
    if (queue_is_full(q)) return false;
    q->items[(q->head + q->size) % QUEUE_CAPACITY] = value;
    q->size++;
    return true;
}

/* queue_dequeue: remove from the front; false when empty. */
static bool queue_dequeue(Queue *q, int *out)
{
    if (queue_is_empty(q)) return false;
    *out = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->size--;
    return true;
}

static void demo_queue(void)
{
    Queue q;
    queue_init(&q);

    for (int i = 1; i <= 4; i++) {
        queue_enqueue(&q, i * 100);
        printf("enqueue %d  ", i * 100);
    }
    printf("\n");

    int value;
    printf("dequeues in FIFO order: ");
    while (queue_dequeue(&q, &value)) {
        printf("%d ", value);
    }
    printf("\n");
}

/* --------------------------- Binary search tree --------------------------- */

/*
 * Each node holds a value; everything smaller lives in the left subtree,
 * everything larger in the right. Search/insert are O(log n) on balanced
 * trees, degrading to O(n) if input arrives sorted.
 */
typedef struct TreeNode {
    int value;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

/*
 * bst_insert
 * ----------
 * Recursively finds the correct empty slot and links a new node there.
 * Returns the (possibly new) subtree root, which makes the recursion
 * elegant: parent->left = bst_insert(parent->left, v).
 * Duplicate values are ignored.
 */
static TreeNode *bst_insert(TreeNode *root, int value)
{
    if (root == NULL) {
        TreeNode *node = malloc(sizeof *node);
        if (node == NULL) return NULL;
        node->value = value;
        node->left = node->right = NULL;
        return node;
    }
    if (value < root->value) {
        root->left = bst_insert(root->left, value);
    } else if (value > root->value) {
        root->right = bst_insert(root->right, value);
    }
    return root;
}

/*
 * bst_contains
 * ------------
 * Walks down the tree choosing left or right at each node — a binary
 * search embodied as a data structure.
 */
static bool bst_contains(const TreeNode *root, int value)
{
    while (root != NULL) {
        if (value == root->value) return true;
        root = (value < root->value) ? root->left : root->right;
    }
    return false;
}

/*
 * bst_print_inorder
 * -----------------
 * In-order traversal (left, node, right) visits BST values in sorted
 * order — a beautiful property worth demonstrating.
 */
static void bst_print_inorder(const TreeNode *root)
{
    if (root == NULL) return;
    bst_print_inorder(root->left);
    printf("%d ", root->value);
    bst_print_inorder(root->right);
}

/*
 * bst_height
 * ----------
 * Height = 1 + max(height(left), height(right)); empty tree is 0.
 */
static int bst_height(const TreeNode *root)
{
    if (root == NULL) return 0;
    return 1 + MAX(bst_height(root->left), bst_height(root->right));
}

/*
 * bst_free
 * --------
 * Post-order (children first) so we never touch a freed node.
 */
static void bst_free(TreeNode *root)
{
    if (root == NULL) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

static void demo_bst(void)
{
    TreeNode *root = NULL;
    int values[] = { 50, 30, 70, 20, 40, 60, 80 };

    for (size_t i = 0; i < ARRAY_LEN(values); i++) {
        root = bst_insert(root, values[i]);
    }
    printf("inserted       : ");
    print_int_array(values, ARRAY_LEN(values));
    printf("\n");

    printf("in-order       : ");
    bst_print_inorder(root);
    printf("(sorted!)\n");

    printf("contains 40    : %s\n", bst_contains(root, 40) ? "yes" : "no");
    printf("contains 55    : %s\n", bst_contains(root, 55) ? "yes" : "no");
    printf("height         : %d\n", bst_height(root));

    bst_free(root);
}

/* ------------------------ Hash table (string -> int) ---------------------- */

/*
 * A hash table with separate chaining: an array of buckets, each a linked
 * list of key/value pairs. Average O(1) lookup as long as the load stays
 * reasonable.
 */
#define HASH_BUCKETS 16

typedef struct HashEntry {
    char *key;                 /* heap-owned copy of the key */
    int value;
    struct HashEntry *next;
} HashEntry;

typedef struct {
    HashEntry *buckets[HASH_BUCKETS];
} HashTable;

/*
 * hash_string
 * -----------
 * The djb2 hash: h = h*33 + c. Simple, fast, and good enough for
 * teaching. Real systems use stronger functions (FNV, SipHash, ...).
 */
static unsigned long hash_string(const char *key)
{
    unsigned long h = 5381;
    for (const unsigned char *p = (const unsigned char *)key; *p != '\0'; p++) {
        h = h * 33 + *p;
    }
    return h;
}

static void hash_init(HashTable *t)
{
    for (size_t i = 0; i < HASH_BUCKETS; i++) {
        t->buckets[i] = NULL;
    }
}

/*
 * hash_put
 * --------
 * Inserts or updates a key. If the key already exists in its bucket's
 * chain, the value is overwritten; otherwise a new entry is prepended.
 */
static bool hash_put(HashTable *t, const char *key, int value)
{
    size_t index = hash_string(key) % HASH_BUCKETS;

    for (HashEntry *e = t->buckets[index]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;       /* key exists: update in place */
            return true;
        }
    }

    HashEntry *e = malloc(sizeof *e);
    if (e == NULL) return false;
    e->key = my_strdup(key);
    if (e->key == NULL) {
        free(e);
        return false;
    }
    e->value = value;
    e->next = t->buckets[index];
    t->buckets[index] = e;
    return true;
}

/*
 * hash_get
 * --------
 * Looks up a key; writes the value through `out` and returns true, or
 * returns false if absent. Separating "found" from the value avoids
 * reserving a magic sentinel value.
 */
static bool hash_get(const HashTable *t, const char *key, int *out)
{
    size_t index = hash_string(key) % HASH_BUCKETS;
    for (HashEntry *e = t->buckets[index]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            *out = e->value;
            return true;
        }
    }
    return false;
}

/*
 * hash_remove
 * -----------
 * Unlinks and frees the entry for `key`, using the same pointer-to-
 * pointer trick as list_delete.
 */
static bool hash_remove(HashTable *t, const char *key)
{
    size_t index = hash_string(key) % HASH_BUCKETS;
    for (HashEntry **cur = &t->buckets[index]; *cur != NULL; cur = &(*cur)->next) {
        if (strcmp((*cur)->key, key) == 0) {
            HashEntry *doomed = *cur;
            *cur = doomed->next;
            free(doomed->key);
            free(doomed);
            return true;
        }
    }
    return false;
}

/* hash_free: frees every chain, including the owned key strings. */
static void hash_free(HashTable *t)
{
    for (size_t i = 0; i < HASH_BUCKETS; i++) {
        HashEntry *e = t->buckets[i];
        while (e != NULL) {
            HashEntry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
        t->buckets[i] = NULL;
    }
}

static void demo_hash_table(void)
{
    HashTable ages;
    hash_init(&ages);

    hash_put(&ages, "alice", 30);
    hash_put(&ages, "bob", 25);
    hash_put(&ages, "carol", 35);
    hash_put(&ages, "bob", 26);          /* update, not duplicate */

    int age;
    if (hash_get(&ages, "bob", &age)) {
        printf("bob   -> %d (updated from 25)\n", age);
    }
    if (hash_get(&ages, "alice", &age)) {
        printf("alice -> %d\n", age);
    }
    printf("dave  -> %s\n", hash_get(&ages, "dave", &age) ? "found" : "not found");

    hash_remove(&ages, "alice");
    printf("alice after remove -> %s\n",
           hash_get(&ages, "alice", &age) ? "found" : "not found");

    hash_free(&ages);
}

/* ===========================================================================
 * CHAPTER 9 — ALGORITHMS: SEARCHING & SORTING
 * ===========================================================================
 */

/*
 * linear_search
 * -------------
 * Scans every element in order. O(n), works on unsorted data.
 * Returns the index of the first match, or -1.
 */
static long linear_search(const int *arr, size_t len, int target)
{
    for (size_t i = 0; i < len; i++) {
        if (arr[i] == target) return (long)i;
    }
    return -1;
}

/*
 * binary_search_int
 * -----------------
 * Repeatedly halves a SORTED range. O(log n) — the payoff for keeping
 * data sorted. Note mid computed as lo + (hi - lo) / 2 to avoid the
 * classic (lo + hi) overflow bug.
 */
static long binary_search_int(const int *arr, size_t len, int target)
{
    size_t lo = 0, hi = len;            /* half-open range [lo, hi) */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return (long)mid;
        if (arr[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return -1;
}

/*
 * bubble_sort
 * -----------
 * Repeatedly swaps adjacent out-of-order pairs; large values "bubble"
 * to the end. O(n^2), but the `swapped` flag gives O(n) best case on
 * already-sorted input. Taught for intuition, not for production.
 */
static void bubble_sort(int *arr, size_t len)
{
    if (len < 2) return;
    bool swapped = true;
    for (size_t pass = 0; swapped && pass < len - 1; pass++) {
        swapped = false;
        for (size_t i = 0; i < len - 1 - pass; i++) {
            if (arr[i] > arr[i + 1]) {
                swap(&arr[i], &arr[i + 1]);
                swapped = true;
            }
        }
    }
}

/*
 * selection_sort
 * --------------
 * Finds the minimum of the unsorted region and swaps it into place.
 * Always O(n^2) comparisons but only O(n) swaps.
 */
static void selection_sort(int *arr, size_t len)
{
    for (size_t i = 0; i + 1 < len; i++) {
        size_t min_index = i;
        for (size_t j = i + 1; j < len; j++) {
            if (arr[j] < arr[min_index]) min_index = j;
        }
        if (min_index != i) {
            swap(&arr[i], &arr[min_index]);
        }
    }
}

/*
 * insertion_sort
 * --------------
 * Grows a sorted prefix by inserting each new element into position,
 * shifting larger elements right. O(n^2) worst case but excellent on
 * small or nearly-sorted input — real libraries use it as a base case.
 */
static void insertion_sort(int *arr, size_t len)
{
    for (size_t i = 1; i < len; i++) {
        int key = arr[i];
        size_t j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

/*
 * merge + merge_sort
 * ------------------
 * Divide and conquer: split in half, sort each half recursively, then
 * merge the two sorted halves. O(n log n) guaranteed, at the cost of a
 * temporary buffer. The first "serious" algorithm most students meet.
 */
static void merge(int *arr, int *tmp, size_t lo, size_t mid, size_t hi)
{
    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        /* <= keeps equal elements in original order (stable sort) */
        tmp[k++] = (arr[i] <= arr[j]) ? arr[i++] : arr[j++];
    }
    while (i < mid) tmp[k++] = arr[i++];
    while (j < hi)  tmp[k++] = arr[j++];
    memcpy(arr + lo, tmp + lo, (hi - lo) * sizeof *arr);
}

static void merge_sort_range(int *arr, int *tmp, size_t lo, size_t hi)
{
    if (hi - lo < 2) return;            /* 0 or 1 element: already sorted */
    size_t mid = lo + (hi - lo) / 2;
    merge_sort_range(arr, tmp, lo, mid);
    merge_sort_range(arr, tmp, mid, hi);
    merge(arr, tmp, lo, mid, hi);
}

static bool merge_sort(int *arr, size_t len)
{
    int *tmp = malloc(len * sizeof *tmp);
    if (tmp == NULL) return false;
    merge_sort_range(arr, tmp, 0, len);
    free(tmp);
    return true;
}

/*
 * compare_ints_asc / compare_ints_desc
 * ------------------------------------
 * Comparator callbacks for the standard library's qsort. qsort hands
 * each comparator two `const void *` pointers; we cast them back to
 * `const int *` and return negative/zero/positive.
 */
static int compare_ints_asc(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);           /* avoids overflow of x - y */
}

static int compare_ints_desc(const void *a, const void *b)
{
    return compare_ints_asc(b, a);      /* just flip the arguments */
}

static void demo_searching(void)
{
    int data[] = { 12, 5, 8, 30, 2, 19 };
    size_t len = ARRAY_LEN(data);

    printf("unsorted data  : ");
    print_int_array(data, len);
    printf("\n");
    printf("linear_search(30)  -> index %ld\n", linear_search(data, len, 30));
    printf("linear_search(99)  -> index %ld (not found)\n", linear_search(data, len, 99));

    insertion_sort(data, len);
    printf("sorted data    : ");
    print_int_array(data, len);
    printf("\n");
    printf("binary_search(19)  -> index %ld\n", binary_search_int(data, len, 19));
    printf("binary_search(3)   -> index %ld (not found)\n", binary_search_int(data, len, 3));
}

static void demo_sorting(void)
{
    const int original[] = { 42, 7, 19, 3, 88, 27, 1, 65 };
    size_t len = ARRAY_LEN(original);
    int work[ARRAY_LEN(original)];

    printf("original       : ");
    print_int_array(original, len);
    printf("\n");

    memcpy(work, original, sizeof original);
    bubble_sort(work, len);
    printf("bubble_sort    : ");
    print_int_array(work, len);
    printf("\n");

    memcpy(work, original, sizeof original);
    selection_sort(work, len);
    printf("selection_sort : ");
    print_int_array(work, len);
    printf("\n");

    memcpy(work, original, sizeof original);
    insertion_sort(work, len);
    printf("insertion_sort : ");
    print_int_array(work, len);
    printf("\n");

    memcpy(work, original, sizeof original);
    merge_sort(work, len);
    printf("merge_sort     : ");
    print_int_array(work, len);
    printf("\n");

    memcpy(work, original, sizeof original);
    qsort(work, len, sizeof work[0], compare_ints_desc);
    printf("qsort (desc)   : ");
    print_int_array(work, len);
    printf("\n");
}

/* ===========================================================================
 * CHAPTER 10 — FILE I/O
 * ===========================================================================
 */

/*
 * write_lines_to_file
 * -------------------
 * Opens a text file for writing ("w" truncates or creates), writes each
 * string as a line with fprintf, and closes it. Returns false and prints
 * the OS error (strerror(errno)) on failure. Forgetting fclose leaks the
 * file handle and can lose buffered data.
 */
static bool write_lines_to_file(const char *path, const char *const *lines,
                                size_t count)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s for writing: %s\n", path, strerror(errno));
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    return true;
}

/*
 * read_file_lines
 * ---------------
 * Reads a text file line by line with fgets into a fixed buffer.
 * fgets keeps the '\n', so we strip it for clean printing. Returns the
 * number of lines read, or -1 on open failure.
 */
static long read_file_lines(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s for reading: %s\n", path, strerror(errno));
        return -1;
    }

    char buffer[256];
    long count = 0;
    while (fgets(buffer, sizeof buffer, f) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';   /* strip trailing newline */
        count++;
        printf("  line %ld: %s\n", count, buffer);
    }
    fclose(f);
    return count;
}

/*
 * append_line_to_file
 * -------------------
 * Mode "a" positions every write at the end of the file, preserving
 * existing contents.
 */
static bool append_line_to_file(const char *path, const char *line)
{
    FILE *f = fopen(path, "a");
    if (f == NULL) return false;
    fprintf(f, "%s\n", line);
    fclose(f);
    return true;
}

/*
 * save_students_binary / load_students_binary
 * -------------------------------------------
 * Binary I/O with fwrite/fread: dumps an array of structs byte-for-byte
 * ("wb"/"rb"). Compact and fast, but NOT portable across machines with
 * different endianness, padding, or struct layout — a key lesson.
 * load returns the number of records actually read.
 */
static bool save_students_binary(const char *path, const Student *students,
                                 size_t count)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    size_t written = fwrite(students, sizeof *students, count, f);
    fclose(f);
    return written == count;
}

static size_t load_students_binary(const char *path, Student *out, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    size_t read_count = fread(out, sizeof *out, max, f);
    fclose(f);
    return read_count;
}

static void demo_file_io(void)
{
    const char *text_path = "course_demo.txt";
    const char *lines[] = {
        "First line written from C.",
        "Files are streams of bytes.",
        "Always check fopen's return value.",
    };

    if (write_lines_to_file(text_path, lines, ARRAY_LEN(lines))) {
        printf("wrote %zu lines to %s\n", ARRAY_LEN(lines), text_path);
    }
    append_line_to_file(text_path, "This line was appended.");

    printf("reading back:\n");
    long n = read_file_lines(text_path);
    printf("read %ld lines total\n", n);

    /* Binary round trip. */
    const char *bin_path = "students.bin";
    Student out[] = {
        { "Alice", 1001, 3.85 },
        { "Bob",   1002, 3.42 },
    };
    Student in[8];

    if (save_students_binary(bin_path, out, ARRAY_LEN(out))) {
        size_t loaded = load_students_binary(bin_path, in, ARRAY_LEN(in));
        printf("binary round trip loaded %zu students:\n", loaded);
        for (size_t i = 0; i < loaded; i++) {
            printf("  ");
            student_print(&in[i]);
        }
    }

    /* Clean up the demo files so repeated runs start fresh. */
    remove(text_path);
    remove(bin_path);
}

/* ===========================================================================
 * CHAPTER 11 — BIT MANIPULATION
 * ===========================================================================
 */

/*
 * set_bit / clear_bit / toggle_bit / test_bit
 * -------------------------------------------
 * The four fundamental single-bit operations, built from a mask
 * (1u << n) combined with OR, AND+NOT, XOR, and AND respectively.
 * These patterns are everywhere in embedded and systems code.
 */
static unsigned set_bit(unsigned value, unsigned n)    { return value | (1u << n); }
static unsigned clear_bit(unsigned value, unsigned n)  { return value & ~(1u << n); }
static unsigned toggle_bit(unsigned value, unsigned n) { return value ^ (1u << n); }
static bool     test_bit(unsigned value, unsigned n)   { return (value >> n) & 1u; }

/*
 * count_set_bits
 * --------------
 * Brian Kernighan's trick: value & (value - 1) clears the lowest set
 * bit, so the loop runs once per 1-bit rather than once per bit.
 */
static unsigned count_set_bits(unsigned value)
{
    unsigned count = 0;
    while (value != 0) {
        value &= value - 1;
        count++;
    }
    return count;
}

/*
 * is_power_of_two
 * ---------------
 * A power of two has exactly one set bit, so v & (v-1) == 0 (excluding 0).
 */
static bool is_power_of_two(unsigned value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

/*
 * print_binary
 * ------------
 * Prints the low `bits` bits of a value, MSB first, by shifting a probe
 * bit across the word.
 */
static void print_binary(unsigned value, unsigned bits)
{
    for (unsigned i = bits; i-- > 0; ) {
        putchar(test_bit(value, i) ? '1' : '0');
    }
}

static void demo_bits(void)
{
    unsigned v = 0;

    printf("start          : ");
    print_binary(v, 8);
    printf(" (%u)\n", v);

    v = set_bit(v, 1);
    v = set_bit(v, 3);
    v = set_bit(v, 5);
    printf("set bits 1,3,5 : ");
    print_binary(v, 8);
    printf(" (%u)\n", v);

    v = clear_bit(v, 3);
    printf("clear bit 3    : ");
    print_binary(v, 8);
    printf(" (%u)\n", v);

    v = toggle_bit(v, 7);
    printf("toggle bit 7   : ");
    print_binary(v, 8);
    printf(" (%u)\n", v);

    printf("bit 5 is %s, bit 6 is %s\n",
           test_bit(v, 5) ? "set" : "clear",
           test_bit(v, 6) ? "set" : "clear");

    printf("count_set_bits(0xFF) = %u\n", count_set_bits(0xFFu));
    printf("is_power_of_two(64) = %s, is_power_of_two(72) = %s\n",
           is_power_of_two(64) ? "true" : "false",
           is_power_of_two(72) ? "true" : "false");

    /* Shifts: << n multiplies by 2^n, >> n divides (for unsigned). */
    printf("1u << 4 = %u, 256u >> 3 = %u\n", 1u << 4, 256u >> 3);

    /* Masking: extract the low byte of a larger value. */
    unsigned word = 0xABCDu;
    printf("0x%X & 0xFF = 0x%X (low byte)\n", word, word & 0xFFu);
}

/* ===========================================================================
 * CHAPTER 12 — ADVANCED TOPICS
 * ===========================================================================
 */

/*
 * sum_variadic
 * ------------
 * A variadic function (like printf) taking a count followed by that many
 * ints. <stdarg.h> provides va_start/va_arg/va_end to walk the untyped
 * argument list. The count is required because C gives variadic
 * functions no way to know how many arguments they received.
 */
static long sum_variadic(int count, ...)
{
    va_list args;
    long total = 0;

    va_start(args, count);
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }
    va_end(args);
    return total;
}

/*
 * next_id
 * -------
 * Demonstrates a `static` local variable: it is initialized once and
 * keeps its value between calls, giving the function memory without
 * global state. (Note: not thread-safe — a good discussion point.)
 */
static int next_id(void)
{
    static int counter = 0;
    return ++counter;
}

/*
 * safe_divide
 * -----------
 * Error handling by status code: the function returns whether it
 * succeeded, and delivers the result through an out-parameter. This
 * separates "the answer" from "did it work", which C has no exceptions
 * to express otherwise.
 */
static bool safe_divide(int numerator, int denominator, int *result)
{
    if (denominator == 0) {
        return false;               /* refuse instead of crashing */
    }
    /* INT_MIN / -1 overflows on two's-complement machines. */
    if (numerator == INT_MIN && denominator == -1) {
        return false;
    }
    *result = numerator / denominator;
    return true;
}

/*
 * generic_swap
 * ------------
 * A type-generic swap using void pointers and memcpy: it swaps `size`
 * raw bytes between two objects. This is exactly how qsort handles
 * arbitrary element types. Uses a small stack buffer for the demo;
 * production versions loop over chunks for arbitrary sizes.
 */
static void generic_swap(void *a, void *b, size_t size)
{
    unsigned char tmp[64];
    if (size > sizeof tmp) {
        fprintf(stderr, "generic_swap: object too large (%zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

/*
 * for_each_int
 * ------------
 * A higher-order iteration helper: applies a callback to every array
 * element. The `context` pointer lets the caller thread extra state
 * through the callback without globals — the standard C callback idiom.
 */
static void for_each_int(int *arr, size_t len,
                         void (*fn)(int *element, void *context), void *context)
{
    for (size_t i = 0; i < len; i++) {
        fn(&arr[i], context);
    }
}

/* Callback: scales an element by the int pointed to by context. */
static void scale_element(int *element, void *context)
{
    int factor = *(int *)context;
    *element *= factor;
}

/* Callback: accumulates elements into the long pointed to by context. */
static void accumulate_element(int *element, void *context)
{
    *(long *)context += *element;
}

static void demo_variadic(void)
{
    printf("sum_variadic(3, 1, 2, 3)          = %ld\n", sum_variadic(3, 1, 2, 3));
    printf("sum_variadic(5, 10, 20, 30, 40, 50) = %ld\n",
           sum_variadic(5, 10, 20, 30, 40, 50));
}

static void demo_static_and_macros(void)
{
    printf("next_id() three times: %d, %d, %d (static keeps state)\n",
           next_id(), next_id(), next_id());
    printf("MAX(3, 7) = %d, MIN(3, 7) = %d\n", MAX(3, 7), MIN(3, 7));

    int arr[] = { 1, 2, 3, 4, 5, 6, 7 };
    printf("ARRAY_LEN(arr) = %zu\n", ARRAY_LEN(arr));
}

static void demo_error_handling(void)
{
    int result;

    if (safe_divide(10, 3, &result)) {
        printf("10 / 3 = %d\n", result);
    }
    if (!safe_divide(10, 0, &result)) {
        printf("10 / 0 rejected safely (no crash)\n");
    }

    /* errno: the C library records WHY a call failed. */
    FILE *f = fopen("/no/such/path/at-all.txt", "r");
    if (f == NULL) {
        printf("fopen failed as expected: errno=%d (%s)\n", errno, strerror(errno));
    } else {
        fclose(f);
    }
}

static void demo_generic_programming(void)
{
    int a = 1, b = 2;
    generic_swap(&a, &b, sizeof a);
    printf("generic_swap ints    : a=%d b=%d\n", a, b);

    double da = 1.5, db = 2.5;
    generic_swap(&da, &db, sizeof da);
    printf("generic_swap doubles : da=%.1f db=%.1f\n", da, db);

    int data[] = { 1, 2, 3, 4 };
    int factor = 10;
    for_each_int(data, ARRAY_LEN(data), scale_element, &factor);
    printf("for_each scale x10   : ");
    print_int_array(data, ARRAY_LEN(data));
    printf("\n");

    long total = 0;
    for_each_int(data, ARRAY_LEN(data), accumulate_element, &total);
    printf("for_each accumulate  : total = %ld\n", total);
}

/* ===========================================================================
 * MAIN — runs every chapter in order
 * ===========================================================================
 */

int main(int argc, char *argv[])
{
    /* Command-line arguments: argv[0] is the program name; the rest are
     * whatever the user typed after it. */
    printf("program invoked as: %s", argv[0]);
    for (int i = 1; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("  (argc = %d)\n", argc);

    CHAPTER("1. Fundamentals: data types");
    demo_data_types();
    CHAPTER("1. Fundamentals: operators");
    demo_operators();
    CHAPTER("1. Fundamentals: control flow");
    demo_control_flow();
    CHAPTER("1. Fundamentals: loops");
    demo_loops();

    CHAPTER("2. Functions & recursion");
    demo_functions();

    CHAPTER("3. Arrays");
    demo_arrays();
    CHAPTER("3. Strings");
    demo_strings();

    CHAPTER("4. Matrices (2D arrays)");
    demo_matrices();

    CHAPTER("5. Pointers");
    demo_pointers();
    CHAPTER("5. Function pointers");
    demo_function_pointers();

    CHAPTER("6. Structs, enums, unions");
    demo_structs();

    CHAPTER("7. Dynamic memory");
    demo_dynamic_memory();

    CHAPTER("8. Data structures: linked list");
    demo_linked_list();
    CHAPTER("8. Data structures: stack");
    demo_stack();
    CHAPTER("8. Data structures: queue");
    demo_queue();
    CHAPTER("8. Data structures: binary search tree");
    demo_bst();
    CHAPTER("8. Data structures: hash table");
    demo_hash_table();

    CHAPTER("9. Algorithms: searching");
    demo_searching();
    CHAPTER("9. Algorithms: sorting");
    demo_sorting();

    CHAPTER("10. File I/O");
    demo_file_io();

    CHAPTER("11. Bit manipulation");
    demo_bits();

    CHAPTER("12. Advanced: variadic functions");
    demo_variadic();
    CHAPTER("12. Advanced: static & macros");
    demo_static_and_macros();
    CHAPTER("12. Advanced: error handling");
    demo_error_handling();
    CHAPTER("12. Advanced: generic programming");
    demo_generic_programming();

    printf("\nAll chapters completed successfully.\n");
    return EXIT_SUCCESS;         /* 0: tell the OS everything went fine */
}
