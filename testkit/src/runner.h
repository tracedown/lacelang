/*
 * runner — dispatch each vector to the appropriate adapter, diff, and report.
 *
 * Handles all four vector types: parse, validate, execute, extension.
 * Execute + extension runners spin up an internal mock HTTP server to
 * serve the vector's `http_mock` queue.
 */

#ifndef RUNNER_H
#define RUNNER_H

#include "manifest.h"
#include "reporter_junit.h"
#include "vector.h"

typedef enum {
    REPORT_TEXT,
    REPORT_TAP,
    REPORT_JUNIT
} report_format_t;

typedef struct {
    const char               *executor_cmd;    /* required unless manifest */
    const executor_manifest_t *manifest;       /* alternate invocation     */
    int                       timeout_seconds;
    report_format_t           report;
    const char               *output_path;     /* junit XML output         */
    junit_reporter_t         *junit;           /* accumulator when JUnit   */
    const char               *certs_dir;       /* TLS scenario cert pack   */
    /* spec §17: feature areas the executor under test does NOT implement.
     * Vectors whose `requires` list intersects this set are skipped with
     * reason "omitted: <feature>". Owned array, NULL-terminated or NULL. */
    const char *const        *omit;
    size_t                    omit_count;
} runner_config_t;

typedef struct {
    size_t total;
    size_t passed;
    size_t failed;
    size_t skipped;
    size_t omitted;   /* subset of skipped — vector skipped-by-design */
} runner_summary_t;

int runner_run_all(const vector_list_t *vectors, const runner_config_t *cfg, runner_summary_t *sum);

#endif
