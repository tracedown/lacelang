/*
 * diff — JSON deep equality with ignore-path support.
 *
 * Ignore paths follow the grammar:
 *   <segment>(.<segment>|[<idx>])*
 * where
 *   <segment> = bare field name  (e.g. `calls`)
 *   [<idx>]   = array index      (e.g. `[0]`)
 *   [*]       = any array index  (wildcard)
 *
 * Examples:
 *   started_at
 *   calls[*].request.body_path
 *   calls[0].assertions[1].actual
 *
 * Default ignore paths are applied unconditionally before the vector's
 * extra paths. See DEFAULT_IGNORES in diff.c.
 */

#ifndef DIFF_H
#define DIFF_H

#include "third_party/cjson/cJSON.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char  *path;     /* JSONPath-ish string for the diff site */
    char  *detail;   /* human message */
} diff_mismatch_t;

typedef struct {
    diff_mismatch_t *items;
    size_t           n;
    size_t           cap;
} diff_report_t;

/* Strip every ignored path from `node` in place. Each ignored path may
 * contain `[*]` wildcards. `extra_ignores` is a NULL-terminated array
 * of additional paths (from vector.expected.ignore). */
void diff_strip_ignores(cJSON *node, const char *const *extra_ignores, size_t extra_n);
void diff_strip_ignores_ex(cJSON *node, const char *const *extra_ignores, size_t extra_n, bool apply_defaults);

/* Compare two JSON trees after ignore stripping. Populate `report` with
 * mismatches. Returns 0 if equal, 1 if mismatches, -1 on internal error. */
int diff_compare(const cJSON *expected, const cJSON *actual, diff_report_t *report);

void diff_report_init(diff_report_t *r);
void diff_report_free(diff_report_t *r);
void diff_report_print(const diff_report_t *r, const char *indent);

#endif
