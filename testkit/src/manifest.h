/*
 * manifest — parse an executor manifest (TOML) per
 * `specs/schemas/executor-manifest.json` and expand placeholders in the
 * per-subcommand templates.
 *
 * Templates reference:
 *   {script}      script file path
 *   {vars}        variable map JSON (run)
 *   {vars_list}   variable name array JSON (validate)
 *   {context}     validator context JSON (validate)
 *   {prev}        previous result JSON (run)
 *   {config}      lace.config path (run, optional)
 *
 * Unused placeholders expand to empty strings and are dropped from argv
 * when the containing token is whitespace-isolated. Tokens that embed a
 * placeholder are kept with the empty substitution (so e.g. `--vars={vars}`
 * becomes `--vars=`).
 */

#ifndef MANIFEST_H
#define MANIFEST_H

#include <stddef.h>

typedef struct {
    /* [executor] */
    char *name;
    char *version;
    char *language;
    char *conforms_to;   /* may be NULL */

    /* [adapter] */
    char *parse_template;
    char *validate_template;
    char *run_template;
    int   timeout_seconds;   /* 0 = use runner default */

    /* [adapter.env] — alternating KEY, VALUE */
    char **env_pairs;
    size_t env_count;        /* number of pairs */

    /* [conformance] — spec §17. Feature areas the executor does NOT
     * implement. The testkit skips vectors whose `requires` intersects
     * this set. Owned array, NULL-terminated. May be empty/NULL. */
    char **omit;
    size_t omit_count;
} executor_manifest_t;

/* Load a manifest from disk. Returns NULL on any failure; errmsg is
 * written to stderr. */
executor_manifest_t *manifest_load(const char *path);

void manifest_free(executor_manifest_t *m);

/* Expand a template into a NULL-terminated argv. Caller frees the returned
 * argv array AND argv[0] (which owns the contiguous string buffer holding
 * every token). Unknown placeholders are silently left as-is. */
char **manifest_expand_template(
    const char *template_str,
    const char *script,
    const char *vars,
    const char *vars_list,
    const char *context,
    const char *prev,
    const char *config
);

/* Free an argv produced by manifest_expand_template. */
void manifest_argv_free(char **argv);

#endif
