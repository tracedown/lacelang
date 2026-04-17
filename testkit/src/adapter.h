/*
 * adapter — invoke an executor subprocess per the CLI contract.
 *
 * Phase 1 only supports `-c <cmd>` with the standard CLI contract
 * (`<cmd> parse|validate|run ...`). TOML manifest loading (`-m`) will
 * land alongside phase-2 work.
 *
 * The adapter writes any ancillary JSON inputs (vars, context, prev) to
 * a temporary directory, invokes the executor with the appropriate
 * arguments, collects stdout + exit code, and returns them.
 */

#ifndef ADAPTER_H
#define ADAPTER_H

#include "manifest.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    /* Inputs — all owned by caller. */
    const char *executor_cmd;      /* e.g. "myexec" or "python -m lace_py"  */
                                   /* Mutually exclusive with manifest.      */
    const executor_manifest_t *manifest;  /* optional — takes precedence     */
    const char *script_path;       /* .lace file path on disk                */
    const char *vars_list_json;    /* for `validate` — JSON array string     */
    const char *context_json;      /* for `validate` — JSON object string    */
    const char *vars_map_json;     /* for `run`      — JSON object string    */
    const char *prev_results_json; /* for `run`      — full JSON or NULL     */
    /* For extension vectors — NULL-terminated array of extension names to
     * activate. Passed as repeated `--enable-extension <name>` flags.
     * Ignored when using a manifest (extensions must be encoded in the
     * template). */
    const char *const *extensions;
    /* Extra environment variables to set on the child subprocess. Format:
     * NULL-terminated array of "KEY=VALUE" strings. Layered on top of the
     * manifest's `[adapter.env]` (if any) and the inherited parent env. */
    const char *const *extra_env;
    /* Extra CLI arguments appended to the executor's `run` invocation
     * (after the existing extras like `--vars`, `--prev`, etc.). NULL-
     * terminated array of strings. Both for `-c <cmd>` and manifest modes. */
    const char *const *extra_cli_args;
    int         timeout_seconds;   /* per-subcall timeout                    */
} adapter_invocation_t;

typedef struct {
    /* Outputs — caller must free(stdout_buf) and free(stderr_buf). */
    int    exit_code;
    bool   timed_out;
    char  *stdout_buf;
    size_t stdout_len;
    char  *stderr_buf;
    size_t stderr_len;
} adapter_result_t;

/* Invoke `<executor_cmd> parse <script_path>`. */
int adapter_run_parse(const adapter_invocation_t *inv, adapter_result_t *out);

/* Invoke `<executor_cmd> validate <script_path> [--vars-list ...] [--context ...]`. */
int adapter_run_validate(const adapter_invocation_t *inv, adapter_result_t *out);

/* Invoke `<executor_cmd> run <script_path> --vars <file> [--prev <file>]`.
 * Phase 2 also threads http_mock + bundled server info; this signature
 * will extend in a compatible way. */
int adapter_run_execute(const adapter_invocation_t *inv, adapter_result_t *out);

/* Free owned buffers in a result. Safe to call on a zeroed struct. */
void adapter_result_free(adapter_result_t *r);

#endif
