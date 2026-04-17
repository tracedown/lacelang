/*
 * reporter_junit — accumulate test case outcomes and emit a JUnit-XML
 * report file. Selected by `--report junit` or `--output <path>`.
 *
 * Output layout mirrors the surviving dialect of "JUnit XML" consumed by
 * GitHub Actions, GitLab CI, Jenkins, and the major Python/JS test tooling.
 */

#ifndef REPORTER_JUNIT_H
#define REPORTER_JUNIT_H

#include <stddef.h>

typedef enum {
    JUNIT_PASSED,
    JUNIT_FAILED,
    JUNIT_SKIPPED,
} junit_outcome_t;

typedef struct junit_reporter junit_reporter_t;

junit_reporter_t *junit_reporter_new(const char *suite_name);
void              junit_reporter_free(junit_reporter_t *r);

/* Record one test case. `message` is shown for failures/skips; may be NULL. */
void junit_reporter_add(
    junit_reporter_t *r,
    const char       *classname,
    const char       *test_name,
    junit_outcome_t   outcome,
    double            elapsed_seconds,
    const char       *message
);

/* Write the accumulated report to `path`. Returns 0 on success. */
int junit_reporter_write(const junit_reporter_t *r, const char *path);

#endif
