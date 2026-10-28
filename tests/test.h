/*
 * A minimal unit test harness: checks count and report, main returns
 * non-zero if any failed.
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int test_checks, test_failures;

#define CHECK(cond) do { \
    test_checks++; \
    if (!(cond)) { \
        test_failures++; \
        fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_INT(a, b) do { \
    long long a_ = (long long)(a), b_ = (long long)(b); \
    test_checks++; \
    if (a_ != b_) { \
        test_failures++; \
        fprintf(stderr, "%s:%d: %s is %lld, expected %lld\n", __FILE__, __LINE__, #a, a_, b_); \
    } \
} while (0)

#define CHECK_STR(a, b) do { \
    const char *a_ = (a), *b_ = (b); \
    test_checks++; \
    if (strcmp(a_, b_) != 0) { \
        test_failures++; \
        fprintf(stderr, "%s:%d: %s is \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #a, a_, b_); \
    } \
} while (0)

static int test_report(const char *name)
{
    printf("%s: %d checks, %d failed\n", name, test_checks, test_failures);
    return test_failures != 0;
}

#endif
