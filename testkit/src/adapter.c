/*
 * adapter — POSIX-only implementation (Linux/WSL/macOS).
 *
 * Uses fork + execvp + pipes. Windows support deferred.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE  /* for mkstemps */

#include "adapter.h"
#include "manifest.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ── growable byte buffer ──────────────────────────────────────────── */

typedef struct { char *data; size_t len, cap; } buf_t;

static int buf_append(buf_t *b, const char *src, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 4096;
        while (new_cap < b->len + n + 1) new_cap *= 2;
        char *nd = realloc(b->data, new_cap);
        if (!nd) return -1;
        b->data = nd; b->cap = new_cap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    b->data[b->len] = 0;
    return 0;
}

/* ── temp file helpers ─────────────────────────────────────────────── */

static int write_temp(const char *prefix, const char *content, char out_path[256]) {
    snprintf(out_path, 256, "/tmp/%s-XXXXXX.json", prefix);
    int fd = mkstemps(out_path, 5);
    if (fd < 0) return -1;
    size_t n = strlen(content);
    ssize_t w = write(fd, content, n);
    close(fd);
    return (w == (ssize_t)n) ? 0 : -1;
}

/* ── argv parser: split a command string on whitespace (simple). ──── */
/* Quoted strings and escapes are NOT supported — executors with spaces
 * in their paths should be invoked via the manifest (phase 2) or a
 * wrapper script. */

static int split_cmd(const char *cmd, char ***out_argv, size_t *out_argc) {
    char *dup = strdup(cmd);
    if (!dup) return -1;
    size_t cap = 8, n = 0;
    char **argv = calloc(cap, sizeof(char*));
    if (!argv) { free(dup); return -1; }
    char *saveptr = NULL;
    char *tok = strtok_r(dup, " \t", &saveptr);
    while (tok) {
        if (n + 2 >= cap) {
            cap *= 2;
            char **na = realloc(argv, cap * sizeof(char*));
            if (!na) { free(dup); free(argv); return -1; }
            argv = na;
        }
        argv[n++] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }
    argv[n] = NULL;
    *out_argv = argv;
    *out_argc = n;
    /* Caller must free argv AND argv[0] (the dup). */
    return 0;
}

/* ── core subprocess runner ────────────────────────────────────────── */

static int run_subprocess(
    const char *const argv[],
    int timeout_seconds,
    char *const env_pairs[],     /* optional — alternating KEY, VAL pairs */
    size_t env_count,            /* number of pairs (not entries)          */
    const char *const *extra_env, /* optional — NULL-terminated KEY=VAL    */
    adapter_result_t *out
) {
    memset(out, 0, sizeof(*out));

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) < 0) return -1;
    if (pipe(err_pipe) < 0) { close(out_pipe[0]); close(out_pipe[1]); return -1; }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* child */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        /* Apply extra environment variables from the manifest. */
        for (size_t i = 0; i < env_count; i++) {
            setenv(env_pairs[i * 2], env_pairs[i * 2 + 1], 1);
        }
        /* Apply per-vector environment overrides (input.env). These are
         * formatted as "KEY=VALUE" pairs; split at the first '='. */
        if (extra_env) {
            for (size_t i = 0; extra_env[i]; i++) {
                const char *eq = strchr(extra_env[i], '=');
                if (!eq) continue;
                size_t klen = (size_t)(eq - extra_env[i]);
                char   key[256];
                if (klen >= sizeof(key)) continue;
                memcpy(key, extra_env[i], klen);
                key[klen] = 0;
                setenv(key, eq + 1, 1);
            }
        }
        execvp(argv[0], (char *const *)argv);
        /* execvp only returns on error */
        fprintf(stderr, "execvp %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    close(out_pipe[1]);
    close(err_pipe[1]);

    /* Set reader ends non-blocking for the timeout loop. */
    int flags;
    flags = fcntl(out_pipe[0], F_GETFL, 0); fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(err_pipe[0], F_GETFL, 0); fcntl(err_pipe[0], F_SETFL, flags | O_NONBLOCK);

    buf_t stdout_buf = {0}, stderr_buf = {0};
    char chunk[4096];

    struct timespec deadline, now;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_seconds > 0 ? timeout_seconds : 60;

    bool child_exited = false;
    int  status = 0;

    while (!child_exited) {
        /* read available bytes */
        ssize_t n;
        while ((n = read(out_pipe[0], chunk, sizeof(chunk))) > 0) buf_append(&stdout_buf, chunk, n);
        while ((n = read(err_pipe[0], chunk, sizeof(chunk))) > 0) buf_append(&stderr_buf, chunk, n);

        /* check exit */
        int r = waitpid(pid, &status, WNOHANG);
        if (r == pid) { child_exited = true; break; }
        if (r < 0 && errno != ECHILD) break;

        /* check timeout */
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec
            || (now.tv_sec == deadline.tv_sec && now.tv_nsec > deadline.tv_nsec)) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            out->timed_out = true;
            break;
        }

        /* small sleep to avoid spinning */
        struct timespec ts = { 0, 10 * 1000 * 1000 };  /* 10ms */
        nanosleep(&ts, NULL);
    }

    /* drain remaining bytes */
    ssize_t n;
    while ((n = read(out_pipe[0], chunk, sizeof(chunk))) > 0) buf_append(&stdout_buf, chunk, n);
    while ((n = read(err_pipe[0], chunk, sizeof(chunk))) > 0) buf_append(&stderr_buf, chunk, n);

    close(out_pipe[0]); close(err_pipe[0]);

    out->stdout_buf = stdout_buf.data;
    out->stdout_len = stdout_buf.len;
    out->stderr_buf = stderr_buf.data;
    out->stderr_len = stderr_buf.len;

    if (out->timed_out) {
        out->exit_code = -1;
    } else if (WIFEXITED(status)) {
        out->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        out->exit_code = 128 + WTERMSIG(status);
    } else {
        out->exit_code = -1;
    }
    return 0;
}

/* ── public subcommand wrappers ────────────────────────────────────── */

/* Invoke the executor via manifest templates. Placeholders in the template
 * expand from the three main payloads the adapter tracks: script path, JSON
 * aux files (written to temp first, then their paths are substituted), and
 * extensions. */
static int invoke_via_manifest(
    const adapter_invocation_t *inv,
    const char *subcommand,
    adapter_result_t *out
) {
    const executor_manifest_t *m = inv->manifest;
    const char *tpl = NULL;
    if      (strcmp(subcommand, "parse")    == 0) tpl = m->parse_template;
    else if (strcmp(subcommand, "validate") == 0) tpl = m->validate_template;
    else if (strcmp(subcommand, "run")      == 0) tpl = m->run_template;
    if (!tpl) return -1;

    char vars_path[256]      = {0};
    char vars_list_path[256] = {0};
    char context_path[256]   = {0};
    char prev_path[256]      = {0};

    /* Always materialise aux files, falling back to empty JSON defaults so
     * manifest templates can reference any placeholder unconditionally. */
    const char *vars_src      = inv->vars_map_json     ? inv->vars_map_json     : "{}";
    const char *vars_list_src = inv->vars_list_json    ? inv->vars_list_json    : "[]";
    const char *context_src   = inv->context_json      ? inv->context_json      : "{}";
    const char *prev_src      = inv->prev_results_json ? inv->prev_results_json : "null";

    if (write_temp("vars",      vars_src,      vars_path)      != 0) return -1;
    if (write_temp("vars-list", vars_list_src, vars_list_path) != 0) goto fail;
    if (write_temp("context",   context_src,   context_path)   != 0) goto fail;
    if (write_temp("prev",      prev_src,      prev_path)      != 0) goto fail;

    int timeout = inv->timeout_seconds > 0 ? inv->timeout_seconds : m->timeout_seconds;

    char **argv = manifest_expand_template(
        tpl,
        inv->script_path,
        vars_path,
        vars_list_path,
        context_path,
        prev_path,
        /* config placeholder unused — testkit doesn't forward lace.config */
        NULL
    );
    if (!argv) goto fail;

    /* Append per-vector extra CLI args (input.cli_args) to the manifest
     * argv. We rebuild the argv into a fresh array so manifest_argv_free
     * still owns argv[0]'s contiguous buffer. The appended tokens are
     * pointed into the caller's string storage — no extra ownership. */
    char **final_argv = argv;
    size_t argc = 0;
    while (argv[argc]) argc++;
    size_t extra_cli_n = 0;
    if (inv->extra_cli_args) {
        while (inv->extra_cli_args[extra_cli_n]) extra_cli_n++;
    }
    if (extra_cli_n > 0) {
        final_argv = calloc(argc + extra_cli_n + 1, sizeof(char *));
        if (!final_argv) { manifest_argv_free(argv); goto fail; }
        for (size_t i = 0; i < argc; i++) final_argv[i] = argv[i];
        for (size_t i = 0; i < extra_cli_n; i++)
            final_argv[argc + i] = (char *)inv->extra_cli_args[i];
        final_argv[argc + extra_cli_n] = NULL;
    }

    int rc = run_subprocess((const char *const *)final_argv, timeout,
                            m->env_pairs, m->env_count,
                            inv->extra_env, out);
    if (final_argv != argv) free(final_argv);
    manifest_argv_free(argv);

    if (vars_path[0])      unlink(vars_path);
    if (vars_list_path[0]) unlink(vars_list_path);
    if (context_path[0])   unlink(context_path);
    if (prev_path[0])      unlink(prev_path);
    return rc;

fail:
    if (vars_path[0])      unlink(vars_path);
    if (vars_list_path[0]) unlink(vars_list_path);
    if (context_path[0])   unlink(context_path);
    if (prev_path[0])      unlink(prev_path);
    return -1;
}

static int invoke_with_subcommand(
    const adapter_invocation_t *inv,
    const char *subcommand,
    const char *extra_args[],   /* NULL-terminated, may be NULL */
    size_t extra_n,
    adapter_result_t *out
) {
    if (inv->manifest) return invoke_via_manifest(inv, subcommand, out);
    if (!inv->executor_cmd) return -1;

    char **base_argv = NULL;
    size_t base_argc = 0;
    if (split_cmd(inv->executor_cmd, &base_argv, &base_argc) != 0) return -1;

    size_t extra_cli_n = 0;
    if (inv->extra_cli_args) {
        while (inv->extra_cli_args[extra_cli_n]) extra_cli_n++;
    }

    /* Build full argv: base + subcommand + script + extras + extra_cli + NULL */
    size_t total = base_argc + 2 + extra_n + extra_cli_n + 1;
    char **argv = calloc(total, sizeof(char*));
    if (!argv) {
        free(base_argv[0]);  /* the strdup'd buffer */
        free(base_argv);
        return -1;
    }
    size_t p = 0;
    for (size_t i = 0; i < base_argc; i++) argv[p++] = base_argv[i];
    argv[p++] = (char*)subcommand;
    argv[p++] = (char*)inv->script_path;
    for (size_t i = 0; i < extra_n; i++) argv[p++] = (char*)extra_args[i];
    for (size_t i = 0; i < extra_cli_n; i++)
        argv[p++] = (char*)inv->extra_cli_args[i];
    argv[p] = NULL;

    int rc = run_subprocess((const char *const *)argv, inv->timeout_seconds,
                            NULL, 0, inv->extra_env, out);

    free(argv);
    free(base_argv[0]);
    free(base_argv);
    return rc;
}

int adapter_run_parse(const adapter_invocation_t *inv, adapter_result_t *out) {
    return invoke_with_subcommand(inv, "parse", NULL, 0, out);
}

int adapter_run_validate(const adapter_invocation_t *inv, adapter_result_t *out) {
    char vars_list_path[256] = {0};
    char context_path[256]   = {0};
    const char *extras[8];
    size_t n = 0;

    if (inv->vars_list_json) {
        if (write_temp("vars-list", inv->vars_list_json, vars_list_path) != 0) return -1;
        extras[n++] = "--vars-list";
        extras[n++] = vars_list_path;
    }
    if (inv->context_json) {
        if (write_temp("context", inv->context_json, context_path) != 0) return -1;
        extras[n++] = "--context";
        extras[n++] = context_path;
    }

    int rc = invoke_with_subcommand(inv, "validate", extras, n, out);

    if (vars_list_path[0]) unlink(vars_list_path);
    if (context_path[0])   unlink(context_path);
    return rc;
}

int adapter_run_execute(const adapter_invocation_t *inv, adapter_result_t *out) {
    char vars_path[256] = {0};
    char prev_path[256] = {0};
    /* Room for --vars/--prev plus a healthy dose of --enable-extension pairs. */
    const char *extras[32];
    size_t n = 0;

    if (inv->vars_map_json) {
        if (write_temp("vars", inv->vars_map_json, vars_path) != 0) return -1;
        extras[n++] = "--vars";
        extras[n++] = vars_path;
    }
    if (inv->prev_results_json) {
        if (write_temp("prev", inv->prev_results_json, prev_path) != 0) return -1;
        extras[n++] = "--prev";
        extras[n++] = prev_path;
    }
    if (inv->extensions) {
        for (size_t i = 0; inv->extensions[i]
                           && n + 1 < sizeof(extras) / sizeof(*extras); i++) {
            extras[n++] = "--enable-extension";
            extras[n++] = inv->extensions[i];
        }
    }

    int rc = invoke_with_subcommand(inv, "run", extras, n, out);

    if (vars_path[0]) unlink(vars_path);
    if (prev_path[0]) unlink(prev_path);
    return rc;
}

void adapter_result_free(adapter_result_t *r) {
    if (!r) return;
    free(r->stdout_buf);
    free(r->stderr_buf);
    r->stdout_buf = r->stderr_buf = NULL;
    r->stdout_len = r->stderr_len = 0;
}
