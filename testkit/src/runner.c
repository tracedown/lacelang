#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE  /* for mkstemps */

#include "runner.h"
#include "adapter.h"
#include "diff.h"
#include "mock_server.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

/* ── writing script to a temp file ─────────────────────────────────── */

static int write_script(const char *source, char out_path[256]) {
    snprintf(out_path, 256, "/tmp/vec-XXXXXX.lace");
    int fd = mkstemps(out_path, 5);
    if (fd < 0) return -1;
    size_t n = strlen(source);
    ssize_t w = write(fd, source, n);
    close(fd);
    return (w == (ssize_t)n) ? 0 : -1;
}

/* Create a fresh temp directory for the vector and return its path in
 * `out_dir` (must be at least 256 bytes). Returns 0 on success. */
static int make_tmpdir(char out_dir[256]) {
    snprintf(out_dir, 256, "/tmp/lace-vector-XXXXXX");
    return mkdtemp(out_dir) ? 0 : -1;
}

/* Write `content` to `path`. Returns 0 on success. */
static int write_file(const char *path, const char *content, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    ssize_t w = write(fd, content, len);
    close(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

/* Recursively remove a directory. Best-effort; ignores individual errors. */
static void rmtree(const char *path) {
    DIR *d = opendir(path);
    if (!d) { rmdir(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[1024];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode)) {
            rmtree(child);
        } else {
            unlink(child);
        }
    }
    closedir(d);
    rmdir(path);
}

/* ── ignore-path extraction ────────────────────────────────────────── */

static size_t extract_ignore_paths(const cJSON *expected, const char ***out_arr) {
    *out_arr = NULL;
    const cJSON *arr = cJSON_GetObjectItem(expected, "ignore");
    if (!arr || !cJSON_IsArray(arr)) return 0;
    int n = cJSON_GetArraySize(arr);
    if (n <= 0) return 0;
    const char **paths = calloc((size_t)n, sizeof(const char *));
    if (!paths) return 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        const cJSON *s = cJSON_GetArrayItem(arr, i);
        if (cJSON_IsString(s)) paths[k++] = s->valuestring;
    }
    *out_arr = paths;
    return (size_t)k;
}

/* ── error-code multiset comparison ────────────────────────────────── */

/* Compare two arrays of {code,...} objects as multisets keyed by code.
 * Only the "code" field is compared; other fields (call_index, field, line)
 * are noted when they disagree but mismatches on them are NOT failures
 * until call_index/chain_method/field checks graduate out of per-executor
 * discretion. For phase 1: only code presence/absence is strict. */
static int diff_error_codes(
    const cJSON *expected_arr,
    const cJSON *actual_arr,
    diff_report_t *r,
    const char *label
) {
    if (!cJSON_IsArray(expected_arr)) expected_arr = NULL;
    if (!cJSON_IsArray(actual_arr))   actual_arr   = NULL;

    int en = expected_arr ? cJSON_GetArraySize(expected_arr) : 0;
    int an = actual_arr   ? cJSON_GetArraySize(actual_arr)   : 0;

    /* For each expected, find a matching actual by code. */
    int *actual_used = calloc((size_t)an, sizeof(int));
    if (!actual_used && an > 0) return -1;

    for (int i = 0; i < en; i++) {
        const cJSON *e = cJSON_GetArrayItem(expected_arr, i);
        const cJSON *e_code = cJSON_GetObjectItem(e, "code");
        if (!cJSON_IsString(e_code)) continue;
        int found = -1;
        for (int j = 0; j < an; j++) {
            if (actual_used[j]) continue;
            const cJSON *a = cJSON_GetArrayItem(actual_arr, j);
            const cJSON *a_code = cJSON_GetObjectItem(a, "code");
            if (cJSON_IsString(a_code) && strcmp(e_code->valuestring, a_code->valuestring) == 0) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            char path[128], detail[256];
            snprintf(path,   sizeof(path),   "%s[%d]", label, i);
            snprintf(detail, sizeof(detail), "expected code %s not emitted by executor", e_code->valuestring);
            /* We need a report_add equivalent: use diff machinery indirectly. */
            /* Since diff.h doesn't expose report_add, stuff the mismatch manually. */
            /* Extend diff_report_t: we cheat via a quick addition: realloc path/detail. */
            if (r->n + 1 > r->cap) {
                size_t nc = r->cap ? r->cap * 2 : 16;
                diff_mismatch_t *ni = realloc(r->items, nc * sizeof(*ni));
                if (!ni) { free(actual_used); return -1; }
                r->items = ni; r->cap = nc;
            }
            r->items[r->n].path   = strdup(path);
            r->items[r->n].detail = strdup(detail);
            r->n++;
        } else {
            actual_used[found] = 1;
        }
    }

    for (int j = 0; j < an; j++) {
        if (actual_used[j]) continue;
        const cJSON *a = cJSON_GetArrayItem(actual_arr, j);
        const cJSON *a_code = cJSON_GetObjectItem(a, "code");
        char path[128], detail[256];
        snprintf(path,   sizeof(path),   "%s[actual %d]", label, j);
        snprintf(detail, sizeof(detail), "unexpected code %s emitted by executor",
                 cJSON_IsString(a_code) ? a_code->valuestring : "(no code)");
        if (r->n + 1 > r->cap) {
            size_t nc = r->cap ? r->cap * 2 : 16;
            diff_mismatch_t *ni = realloc(r->items, nc * sizeof(*ni));
            if (!ni) { free(actual_used); return -1; }
            r->items = ni; r->cap = nc;
        }
        r->items[r->n].path   = strdup(path);
        r->items[r->n].detail = strdup(detail);
        r->n++;
    }

    free(actual_used);
    return r->n ? 1 : 0;
}

/* ── reporting ─────────────────────────────────────────────────────── */

/* Textual summary of a mismatch list for JUnit's <failure message="..."/>. */
static char *diff_report_to_message(const diff_report_t *r, const char *stage) {
    size_t cap = 512;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    int n = snprintf(buf, cap, "%s", stage ? stage : "fail");
    size_t w = (n > 0) ? (size_t)n : 0;
    if (r) {
        for (size_t i = 0; i < r->n; i++) {
            size_t need = w + strlen(r->items[i].path) + strlen(r->items[i].detail) + 8;
            if (need > cap) {
                cap = need + 256;
                char *rn = realloc(buf, cap);
                if (!rn) return buf;
                buf = rn;
            }
            w += (size_t)snprintf(buf + w, cap - w, "\n%s: %s",
                                  r->items[i].path, r->items[i].detail);
        }
    }
    return buf;
}

static void report_pass(const vector_t *v, const runner_config_t *cfg) {
    switch (cfg->report) {
    case REPORT_TAP:   printf("ok - %s\n", v->id); break;
    case REPORT_JUNIT: break;
    case REPORT_TEXT:
    default:           printf("  ok: %s\n", v->rel); break;
    }
    if (cfg->junit) junit_reporter_add(cfg->junit, vector_type_name(v->type), v->id, JUNIT_PASSED, 0.0, NULL);
}

static void report_skip(const vector_t *v, const char *reason, const runner_config_t *cfg) {
    switch (cfg->report) {
    case REPORT_TAP:   printf("ok - %s # SKIP %s\n", v->id, reason); break;
    case REPORT_JUNIT: break;
    case REPORT_TEXT:
    default:           printf("  skip: %s (%s)\n", v->rel, reason); break;
    }
    if (cfg->junit) junit_reporter_add(cfg->junit, vector_type_name(v->type), v->id, JUNIT_SKIPPED, 0.0, reason);
}

static void report_fail(const vector_t *v, const char *stage,
                        const diff_report_t *r, const runner_config_t *cfg) {
    switch (cfg->report) {
    case REPORT_TAP:
        printf("not ok - %s # %s\n", v->id, stage);
        if (r) diff_report_print(r, "#   ");
        break;
    case REPORT_JUNIT:
        break;
    case REPORT_TEXT:
    default:
        printf("FAIL: %s\n        %s (%s)\n", v->rel, v->description, stage);
        if (r) diff_report_print(r, "        ");
        break;
    }
    if (cfg->junit) {
        char *msg = diff_report_to_message(r, stage);
        junit_reporter_add(cfg->junit, vector_type_name(v->type), v->id, JUNIT_FAILED, 0.0, msg);
        free(msg);
    }
}

/* ── per-type runners ──────────────────────────────────────────────── */

static int run_parse_vector(
    const vector_t *v,
    const runner_config_t *cfg
) {
    const cJSON *in  = vector_input(v);
    const cJSON *exp = vector_expected(v);
    if (!in || !exp) return -1;

    const cJSON *src = cJSON_GetObjectItem(in, "source");
    if (!cJSON_IsString(src)) return -1;

    char script_path[256];
    if (write_script(src->valuestring, script_path) != 0) return -1;

    adapter_invocation_t inv = {0};
    inv.executor_cmd     = cfg->executor_cmd;
    inv.manifest             = cfg->manifest;
    inv.script_path      = script_path;
    inv.timeout_seconds  = cfg->timeout_seconds;

    adapter_result_t res = {0};
    int rc = adapter_run_parse(&inv, &res);
    unlink(script_path);
    if (rc != 0) { adapter_result_free(&res); return -1; }

    if (res.timed_out) {
        diff_report_t rep; diff_report_init(&rep);
        /* fabricate a mismatch record */
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) {
            rep.items[0].path   = strdup("(subprocess)");
            rep.items[0].detail = strdup("executor timed out");
            rep.n = rep.cap = 1;
        }
        report_fail(v, "timeout", &rep, cfg);
        diff_report_free(&rep);
        adapter_result_free(&res);
        return 1;
    }

    /* Parse executor stdout as JSON */
    cJSON *actual = NULL;
    if (res.stdout_len > 0) actual = cJSON_ParseWithLength(res.stdout_buf, res.stdout_len);

    if (!actual) {
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) {
            rep.items[0].path   = strdup("(stdout)");
            rep.items[0].detail = strdup("executor stdout is not valid JSON");
            rep.n = rep.cap = 1;
        }
        report_fail(v, "invalid-json", &rep, cfg);
        diff_report_free(&rep);
        adapter_result_free(&res);
        return 1;
    }

    /* Expected may have "ast" or "errors". */
    const cJSON *e_ast    = cJSON_GetObjectItem(exp, "ast");
    const cJSON *e_errors = cJSON_GetObjectItem(exp, "errors");
    cJSON *a_ast    = cJSON_GetObjectItem(actual, "ast");
    cJSON *a_errors = cJSON_GetObjectItem(actual, "errors");

    diff_report_t rep; diff_report_init(&rep);
    int result = 0;

    if (cJSON_IsObject(e_ast) && cJSON_IsObject(a_ast)) {
        const char **ig = NULL;
        size_t ign = extract_ignore_paths(exp, &ig);
        cJSON *exp_copy = cJSON_Duplicate(e_ast, 1);
        cJSON *act_copy = cJSON_Duplicate(a_ast, 1);
        diff_strip_ignores(exp_copy, ig, ign);
        diff_strip_ignores(act_copy, ig, ign);
        result = diff_compare(exp_copy, act_copy, &rep);
        cJSON_Delete(exp_copy);
        cJSON_Delete(act_copy);
        free(ig);
    } else if (cJSON_IsArray(e_errors)) {
        /* Expected: parse errors. Check actual is also an error shape. */
        if (!cJSON_IsArray(a_errors)) {
            if (rep.n + 1 > rep.cap) {
                rep.cap = 4;
                rep.items = calloc(rep.cap, sizeof(diff_mismatch_t));
            }
            rep.items[rep.n].path   = strdup("(shape)");
            rep.items[rep.n].detail = strdup("expected parse errors but executor returned an AST");
            rep.n++;
            result = 1;
        } else {
            result = diff_error_codes(e_errors, a_errors, &rep, "errors");
        }
    } else if (cJSON_IsObject(e_ast) && !cJSON_IsObject(a_ast)) {
        if (rep.n + 1 > rep.cap) {
            rep.cap = 4;
            rep.items = calloc(rep.cap, sizeof(diff_mismatch_t));
        }
        rep.items[rep.n].path   = strdup("(shape)");
        rep.items[rep.n].detail = strdup(cJSON_IsArray(a_errors)
            ? "expected an AST but executor returned parse errors"
            : "expected an AST but actual output lacks both 'ast' and 'errors'");
        rep.n++;
        result = 1;
    } else {
        if (rep.n + 1 > rep.cap) {
            rep.cap = 4;
            rep.items = calloc(rep.cap, sizeof(diff_mismatch_t));
        }
        rep.items[rep.n].path   = strdup("(vector)");
        rep.items[rep.n].detail = strdup("expected block has neither 'ast' nor 'errors'");
        rep.n++;
        result = 1;
    }

    if (result == 0) report_pass(v, cfg);
    else             report_fail(v, "parse-diff", &rep, cfg);

    diff_report_free(&rep);
    cJSON_Delete(actual);
    adapter_result_free(&res);
    return result;
}

static int run_validate_vector(
    const vector_t *v,
    const runner_config_t *cfg
) {
    const cJSON *in  = vector_input(v);
    const cJSON *exp = vector_expected(v);
    if (!in || !exp) return -1;

    const cJSON *src = cJSON_GetObjectItem(in, "source");
    if (!cJSON_IsString(src)) return -1;

    char script_path[256];
    if (write_script(src->valuestring, script_path) != 0) return -1;

    /* Serialize optional vars-list and context for the adapter. */
    char *vars_list_json = NULL;
    const cJSON *vars = cJSON_GetObjectItem(in, "variables");
    if (cJSON_IsArray(vars)) vars_list_json = cJSON_PrintUnformatted(vars);

    char *context_json = NULL;
    const cJSON *ctx = cJSON_GetObjectItem(in, "context");
    if (cJSON_IsObject(ctx)) context_json = cJSON_PrintUnformatted(ctx);

    adapter_invocation_t inv = {0};
    inv.executor_cmd     = cfg->executor_cmd;
    inv.manifest             = cfg->manifest;
    inv.script_path      = script_path;
    inv.vars_list_json   = vars_list_json;
    inv.context_json     = context_json;
    inv.timeout_seconds  = cfg->timeout_seconds;

    adapter_result_t res = {0};
    int rc = adapter_run_validate(&inv, &res);
    unlink(script_path);
    free(vars_list_json);
    free(context_json);
    if (rc != 0) { adapter_result_free(&res); return -1; }

    if (res.timed_out) {
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) { rep.items[0].path = strdup("(subprocess)"); rep.items[0].detail = strdup("executor timed out"); rep.n = rep.cap = 1; }
        report_fail(v, "timeout", &rep, cfg);
        diff_report_free(&rep);
        adapter_result_free(&res);
        return 1;
    }

    cJSON *actual = NULL;
    if (res.stdout_len > 0) actual = cJSON_ParseWithLength(res.stdout_buf, res.stdout_len);
    if (!actual) {
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) { rep.items[0].path = strdup("(stdout)"); rep.items[0].detail = strdup("executor stdout is not valid JSON"); rep.n = rep.cap = 1; }
        report_fail(v, "invalid-json", &rep, cfg);
        diff_report_free(&rep);
        adapter_result_free(&res);
        return 1;
    }

    const cJSON *e_errors   = cJSON_GetObjectItem(exp,    "errors");
    const cJSON *e_warnings = cJSON_GetObjectItem(exp,    "warnings");
    cJSON       *a_errors   = cJSON_GetObjectItem(actual, "errors");
    cJSON       *a_warnings = cJSON_GetObjectItem(actual, "warnings");

    diff_report_t rep; diff_report_init(&rep);
    int r1 = diff_error_codes(e_errors,   a_errors,   &rep, "errors");
    int r2 = diff_error_codes(e_warnings, a_warnings, &rep, "warnings");
    int result = (r1 || r2) ? 1 : 0;

    if (result == 0) report_pass(v, cfg);
    else             report_fail(v, "validate-diff", &rep, cfg);

    diff_report_free(&rep);
    cJSON_Delete(actual);
    adapter_result_free(&res);
    return result;
}

/* ── execute / extension runners ──────────────────────────────────── */

/* Replace every occurrence of "{port}" in `src` with the decimal port and
 * return a freshly-allocated copy. Caller frees. */
static char *substitute_port(const char *src, int port) {
    char port_buf[16];
    snprintf(port_buf, sizeof(port_buf), "%d", port);
    const char *token = "{port}";
    size_t tlen = strlen(token);
    size_t plen = strlen(port_buf);
    size_t slen = strlen(src);

    /* Over-allocate generously: replacement is at most plen per occurrence. */
    size_t cap = slen + 64;
    char  *out = malloc(cap + 1);
    if (!out) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < slen; ) {
        if (i + tlen <= slen && memcmp(src + i, token, tlen) == 0) {
            if (w + plen + 1 > cap) {
                cap = (cap + plen + 64) * 2;
                char *r = realloc(out, cap + 1);
                if (!r) { free(out); return NULL; }
                out = r;
            }
            memcpy(out + w, port_buf, plen);
            w += plen;
            i += tlen;
        } else {
            if (w + 2 > cap) {
                cap = cap * 2;
                char *r = realloc(out, cap + 1);
                if (!r) { free(out); return NULL; }
                out = r;
            }
            out[w++] = src[i++];
        }
    }
    out[w] = 0;
    return out;
}

/* Recursively walk a cJSON tree, replacing "{port}" in every string value. */
static void substitute_port_in_json(cJSON *node, int port) {
    if (!node) return;
    if (cJSON_IsString(node) && node->valuestring) {
        if (strstr(node->valuestring, "{port}")) {
            char *sub = substitute_port(node->valuestring, port);
            if (sub) {
                cJSON_SetValuestring(node, sub);
                free(sub);
            }
        }
        return;
    }
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, node) {
        substitute_port_in_json(child, port);
    }
}

/* Walk the expected tree mirroring the actual tree. For every leaf string in
 * expected that equals "IGNORED" or "NON_NULL", mutate the expected tree so
 * the diff will match:
 *   "IGNORED"  → replace with a deep copy of the corresponding actual node
 *                (always matches).
 *   "NON_NULL" → if actual is non-null, replace expected with a copy of it;
 *                if actual is null, leave "NON_NULL" in place so the diff
 *                fails meaningfully.
 */
static void apply_wildcard_sentinels(cJSON *expected, const cJSON *actual) {
    if (!expected || !actual) return;
    if (cJSON_IsString(expected) && expected->valuestring) {
        if (strcmp(expected->valuestring, "IGNORED") == 0) {
            cJSON *copy = cJSON_Duplicate(actual, 1);
            if (copy) cJSON_ReplaceItemViaPointer(expected->child ? expected : NULL, expected, copy);
            /* ReplaceItemViaPointer only works on container items — handle
             * leaf-level replacement by copying into expected in-place. */
            /* Simpler approach: set fields via cJSON helper below. */
            /* Fall through to custom leaf-replace below. */
        }
        if (strcmp(expected->valuestring, "IGNORED") == 0
                || (strcmp(expected->valuestring, "NON_NULL") == 0 && !cJSON_IsNull(actual))) {
            /* Swap expected's content with a duplicate of actual. */
            cJSON *copy = cJSON_Duplicate(actual, 1);
            if (!copy) return;
            /* Overwrite expected's type + payload fields. */
            if (expected->valuestring) {
                free(expected->valuestring);
                expected->valuestring = NULL;
            }
            expected->type = copy->type;
            expected->valueint    = copy->valueint;
            expected->valuedouble = copy->valuedouble;
            expected->valuestring = copy->valuestring ? strdup(copy->valuestring) : NULL;
            /* Children: splice */
            cJSON *ch = copy->child;
            copy->child = NULL;
            /* Free existing children of expected */
            cJSON *old = expected->child;
            while (old) { cJSON *nx = old->next; cJSON_Delete(old); old = nx; }
            expected->child = ch;
            cJSON *walker = ch;
            while (walker) { walker->prev = walker->prev; walker = walker->next; }
            cJSON_Delete(copy);
            return;
        }
    }
    if (cJSON_IsObject(expected) && cJSON_IsObject(actual)) {
        cJSON *e_child = expected->child;
        while (e_child) {
            cJSON *a_child = cJSON_GetObjectItem(actual, e_child->string);
            apply_wildcard_sentinels(e_child, a_child);
            e_child = e_child->next;
        }
    } else if (cJSON_IsArray(expected) && cJSON_IsArray(actual)) {
        int en = cJSON_GetArraySize(expected);
        int an = cJSON_GetArraySize(actual);
        int m = en < an ? en : an;
        for (int i = 0; i < m; i++) {
            apply_wildcard_sentinels(
                cJSON_GetArrayItem(expected, i),
                cJSON_GetArrayItem(actual, i)
            );
        }
    }
}

/* Build a mock_server response queue from the vector's http_mock array. */
static mock_response_t *build_mock_queue(const cJSON *mock_arr, size_t *out_count) {
    *out_count = 0;
    if (!cJSON_IsArray(mock_arr)) return NULL;
    int n = cJSON_GetArraySize(mock_arr);
    if (n <= 0) return NULL;
    mock_response_t *q = calloc((size_t)n, sizeof(*q));
    if (!q) return NULL;
    for (int i = 0; i < n; i++) {
        const cJSON *m = cJSON_GetArrayItem(mock_arr, i);
        const cJSON *oc = cJSON_GetObjectItem(m, "outcome");
        mock_response_t *r = &q[i];
        if (cJSON_IsString(oc) && strcmp(oc->valuestring, "timeout") == 0) {
            r->outcome = MOCK_OUTCOME_TIMEOUT;
            continue;
        }
        r->outcome = MOCK_OUTCOME_RESPONSE;
        const cJSON *st = cJSON_GetObjectItem(m, "status");
        r->status = cJSON_IsNumber(st) ? (int)st->valuedouble : 200;
        const cJSON *stt = cJSON_GetObjectItem(m, "status_text");
        if (cJSON_IsString(stt)) r->status_text = strdup(stt->valuestring);
        /* Vector convenience field: redirect_to → adds a Location header
         * and marks the response sticky (serves all subsequent requests
         * until the executor's redirect limit fires). */
        const cJSON *rt = cJSON_GetObjectItem(m, "redirect_to");
        const cJSON *hs = cJSON_GetObjectItem(m, "headers");
        int hc = 0;
        if (cJSON_IsObject(hs)) {
            for (cJSON *h = hs->child; h; h = h->next) if (h->string) hc++;
        }
        int has_location_in_headers = 0;
        if (cJSON_IsObject(hs)) {
            for (cJSON *h = hs->child; h; h = h->next) {
                if (h->string && strcasecmp(h->string, "Location") == 0) {
                    has_location_in_headers = 1; break;
                }
            }
        }
        int needs_location = cJSON_IsString(rt) && !has_location_in_headers;
        int total = hc + (needs_location ? 1 : 0);
        if (total > 0) {
            r->headers = calloc((size_t)total * 2, sizeof(char *));
            r->header_count = (size_t)total;
            int k = 0;
            if (cJSON_IsObject(hs)) {
                for (cJSON *h = hs->child; h; h = h->next) {
                    if (!h->string || !cJSON_IsString(h)) continue;
                    r->headers[k * 2]     = strdup(h->string);
                    r->headers[k * 2 + 1] = strdup(h->valuestring);
                    k++;
                }
            }
            if (needs_location) {
                r->headers[k * 2]     = strdup("Location");
                r->headers[k * 2 + 1] = strdup(rt->valuestring);
            }
        }
        if (cJSON_IsString(rt)) r->sticky = 1;

        /* Optional delays: delay_ms (whole-response) or ttfb_delay_ms
         * (treated as same here; we don't model first-byte vs transfer). */
        const cJSON *d1 = cJSON_GetObjectItem(m, "delay_ms");
        const cJSON *d2 = cJSON_GetObjectItem(m, "ttfb_delay_ms");
        if (cJSON_IsNumber(d1)) r->delay_ms = (int)d1->valuedouble;
        else if (cJSON_IsNumber(d2)) r->delay_ms = (int)d2->valuedouble;

        const cJSON *body = cJSON_GetObjectItem(m, "body");
        if (cJSON_IsString(body)) {
            r->body_len = strlen(body->valuestring);
            r->body = malloc(r->body_len + 1);
            if (r->body) memcpy(r->body, body->valuestring, r->body_len + 1);
        }
    }
    *out_count = (size_t)n;
    return q;
}

/* Collect extension names from vector.input.extensions or context.extensions. */
static const char **collect_extensions(const cJSON *in) {
    const cJSON *ext = cJSON_GetObjectItem(in, "extensions");
    if (!cJSON_IsArray(ext)) {
        const cJSON *ctx = cJSON_GetObjectItem(in, "context");
        if (cJSON_IsObject(ctx)) ext = cJSON_GetObjectItem(ctx, "extensions");
    }
    if (!cJSON_IsArray(ext)) return NULL;
    int n = cJSON_GetArraySize(ext);
    if (n <= 0) return NULL;
    const char **out = calloc((size_t)n + 1, sizeof(char *));
    if (!out) return NULL;
    int k = 0;
    for (int i = 0; i < n; i++) {
        const cJSON *s = cJSON_GetArrayItem(ext, i);
        if (cJSON_IsString(s)) out[k++] = s->valuestring;
    }
    out[k] = NULL;
    return out;
}

static int run_execute_vector_common(
    const vector_t *v,
    const runner_config_t *cfg,
    bool extension_vector
) {
    const cJSON *in  = vector_input(v);
    const cJSON *exp = vector_expected(v);
    if (!in || !exp) return -1;

    const cJSON *src = cJSON_GetObjectItem(in, "source");
    if (!cJSON_IsString(src)) return -1;

    /* Start mock server — HTTPS when the vector declares tls_scenario. */
    size_t mock_count = 0;
    mock_response_t *mock_q = build_mock_queue(
        cJSON_GetObjectItem(in, "http_mock"), &mock_count
    );

    const cJSON *tls_s = cJSON_GetObjectItem(in, "tls_scenario");
    mock_server_t *ms = NULL;
    if (cJSON_IsString(tls_s)) {
        const char *scenario = tls_s->valuestring;
        char cert_path[4096], key_path[4096];
        snprintf(cert_path, sizeof(cert_path), "%s/%s.pem", cfg->certs_dir, scenario);
        snprintf(key_path,  sizeof(key_path),  "%s/%s.key", cfg->certs_dir, scenario);
        ms = mock_server_start_tls(mock_q, mock_count, cert_path, key_path);
    } else {
        ms = mock_server_start(mock_q, mock_count);
    }
    if (!ms) {
        /* mock_server takes ownership of queue; if failed we free here. */
        if (mock_q) {
            for (size_t i = 0; i < mock_count; i++) {
                free(mock_q[i].status_text);
                if (mock_q[i].headers) {
                    for (size_t k = 0; k < mock_q[i].header_count * 2; k++)
                        free(mock_q[i].headers[k]);
                    free(mock_q[i].headers);
                }
                free(mock_q[i].body);
            }
            free(mock_q);
        }
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) {
            rep.items[0].path   = strdup("(mock-server)");
            rep.items[0].detail = strdup("failed to start mock server");
            rep.n = rep.cap = 1;
        }
        report_fail(v, "mock-server", &rep, cfg);
        diff_report_free(&rep);
        return 1;
    }

    int port = mock_server_port(ms);

    /* Create per-vector temp directory so that `lace.config` can live beside
     * the script file. Layout:
     *     /tmp/lace-vector-XXXXXX/
     *         script.lace
     *         lace.config   (only if input.lace_config is set)
     */
    char tmp_dir[256];
    if (make_tmpdir(tmp_dir) != 0) { mock_server_stop(ms); return -1; }

    /* Substitute {port} in source and write script.lace. */
    char *rewritten = substitute_port(src->valuestring, port);
    if (!rewritten) { rmtree(tmp_dir); mock_server_stop(ms); return -1; }
    char script_path[512];
    snprintf(script_path, sizeof(script_path), "%s/script.lace", tmp_dir);
    int wrc = write_file(script_path, rewritten, strlen(rewritten));
    free(rewritten);
    if (wrc != 0) { rmtree(tmp_dir); mock_server_stop(ms); return -1; }

    /* Optional `input.lace_config`: write beside the script as lace.config.
     * Apply {port} substitution to the TOML text for parity with source. */
    const cJSON *cfg_node = cJSON_GetObjectItem(in, "lace_config");
    if (cJSON_IsString(cfg_node)) {
        char *cfg_text = substitute_port(cfg_node->valuestring, port);
        if (cfg_text) {
            char cfg_path[512];
            snprintf(cfg_path, sizeof(cfg_path), "%s/lace.config", tmp_dir);
            (void)write_file(cfg_path, cfg_text, strlen(cfg_text));
            free(cfg_text);
        }
    }

    /* Serialize variables map if present, with {port} substitution applied
     * recursively to the values (vector authors often parameterise with a
     * BASE_URL like "http://127.0.0.1:{port}"). */
    char *vars_map_json = NULL;
    const cJSON *vars = cJSON_GetObjectItem(in, "variables");
    if (cJSON_IsObject(vars)) {
        cJSON *vars_copy = cJSON_Duplicate(vars, 1);
        substitute_port_in_json(vars_copy, port);
        vars_map_json = cJSON_PrintUnformatted(vars_copy);
        cJSON_Delete(vars_copy);
    }

    /* Serialize prev_results if present (no port substitution — prev results
     * predate the current run and reference no live ports). */
    char *prev_json = NULL;
    const cJSON *prev = cJSON_GetObjectItem(in, "prev_results");
    if (cJSON_IsObject(prev)) prev_json = cJSON_PrintUnformatted(prev);

    const char **extensions = extension_vector ? collect_extensions(in) : NULL;

    /* Optional `input.env`: build a NULL-terminated array of "KEY=VALUE"
     * strings for the adapter to setenv() in the child. */
    char **extra_env = NULL;
    const cJSON *env_node = cJSON_GetObjectItem(in, "env");
    if (cJSON_IsObject(env_node)) {
        int envc = 0;
        for (cJSON *e = env_node->child; e; e = e->next) {
            if (e->string && cJSON_IsString(e)) envc++;
        }
        if (envc > 0) {
            extra_env = calloc((size_t)envc + 1, sizeof(char *));
            if (extra_env) {
                int k = 0;
                for (cJSON *e = env_node->child; e; e = e->next) {
                    if (!e->string || !cJSON_IsString(e)) continue;
                    size_t kl = strlen(e->string);
                    size_t vl = strlen(e->valuestring);
                    char *kv = malloc(kl + 1 + vl + 1);
                    if (!kv) continue;
                    memcpy(kv, e->string, kl);
                    kv[kl] = '=';
                    memcpy(kv + kl + 1, e->valuestring, vl);
                    kv[kl + 1 + vl] = 0;
                    extra_env[k++] = kv;
                }
                extra_env[k] = NULL;
            }
        }
    }

    /* Optional `input.cli_args`: build a NULL-terminated array of strings.
     * Substitute `{script_dir}` → tmp_dir and `{port}` → port. */
    char **extra_cli_args = NULL;
    const cJSON *cli_node = cJSON_GetObjectItem(in, "cli_args");
    if (cJSON_IsArray(cli_node)) {
        int cn = cJSON_GetArraySize(cli_node);
        if (cn > 0) {
            extra_cli_args = calloc((size_t)cn + 1, sizeof(char *));
            if (extra_cli_args) {
                int k = 0;
                for (int i = 0; i < cn; i++) {
                    const cJSON *s = cJSON_GetArrayItem(cli_node, i);
                    if (!cJSON_IsString(s)) continue;
                    /* First substitute {port}, then {script_dir}. */
                    char *step1 = substitute_port(s->valuestring, port);
                    if (!step1) continue;
                    const char *tok = "{script_dir}";
                    size_t tlen = strlen(tok);
                    size_t dlen = strlen(tmp_dir);
                    size_t slen = strlen(step1);
                    /* Count occurrences so we can allocate exactly. */
                    size_t occ = 0;
                    for (size_t p = 0; p + tlen <= slen; ) {
                        if (memcmp(step1 + p, tok, tlen) == 0) { occ++; p += tlen; }
                        else p++;
                    }
                    size_t out_cap = slen + (dlen > tlen ? (dlen - tlen) * occ : 0) + 1;
                    char *buf = malloc(out_cap);
                    if (!buf) { free(step1); continue; }
                    size_t w = 0;
                    for (size_t p = 0; p < slen; ) {
                        if (p + tlen <= slen && memcmp(step1 + p, tok, tlen) == 0) {
                            memcpy(buf + w, tmp_dir, dlen);
                            w += dlen;
                            p += tlen;
                        } else {
                            buf[w++] = step1[p++];
                        }
                    }
                    buf[w] = 0;
                    free(step1);
                    extra_cli_args[k++] = buf;
                }
                extra_cli_args[k] = NULL;
            }
        }
    }

    adapter_invocation_t inv = {0};
    inv.executor_cmd      = cfg->executor_cmd;
    inv.manifest              = cfg->manifest;
    inv.script_path       = script_path;
    inv.vars_map_json     = vars_map_json;
    inv.prev_results_json = prev_json;
    inv.extensions        = extensions;
    inv.extra_env         = (const char *const *)extra_env;
    inv.extra_cli_args    = (const char *const *)extra_cli_args;
    inv.timeout_seconds   = cfg->timeout_seconds;

    adapter_result_t res = {0};
    int rc = adapter_run_execute(&inv, &res);

    /* Clean up the entire tmp directory (script + lace.config). */
    rmtree(tmp_dir);
    free(vars_map_json);
    free(prev_json);
    free(extensions);
    if (extra_env) {
        for (size_t i = 0; extra_env[i]; i++) free(extra_env[i]);
        free(extra_env);
    }
    if (extra_cli_args) {
        for (size_t i = 0; extra_cli_args[i]; i++) free(extra_cli_args[i]);
        free(extra_cli_args);
    }
    mock_server_stop(ms);

    if (rc != 0) { adapter_result_free(&res); return -1; }

    if (res.timed_out) {
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) {
            rep.items[0].path   = strdup("(subprocess)");
            rep.items[0].detail = strdup("executor timed out");
            rep.n = rep.cap = 1;
        }
        report_fail(v, "timeout", &rep, cfg);
        diff_report_free(&rep);
        adapter_result_free(&res);
        return 1;
    }

    cJSON *actual = NULL;
    if (res.stdout_len > 0) actual = cJSON_ParseWithLength(res.stdout_buf, res.stdout_len);
    if (!actual) {
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(2, sizeof(diff_mismatch_t));
        if (rep.items) {
            rep.items[0].path = strdup("(stdout)");
            rep.items[0].detail = strdup("executor stdout is not valid JSON");
            char detail[8192];
            snprintf(detail, sizeof(detail), "stderr: %.7168s",
                     res.stderr_buf ? res.stderr_buf : "(empty)");
            rep.items[1].path = strdup("(stderr)");
            rep.items[1].detail = strdup(detail);
            rep.n = rep.cap = 2;
        }
        report_fail(v, "invalid-json", &rep, cfg);
        diff_report_free(&rep);
        adapter_result_free(&res);
        return 1;
    }

    const cJSON *e_result = cJSON_GetObjectItem(exp, "result");
    if (!cJSON_IsObject(e_result)) {
        diff_report_t rep; diff_report_init(&rep);
        rep.items = calloc(1, sizeof(diff_mismatch_t));
        if (rep.items) {
            rep.items[0].path = strdup("(vector)");
            rep.items[0].detail = strdup("expected.result is missing or not an object");
            rep.n = rep.cap = 1;
        }
        report_fail(v, "vector-shape", &rep, cfg);
        diff_report_free(&rep);
        cJSON_Delete(actual);
        adapter_result_free(&res);
        return 1;
    }

    /* Clone expected + actual so we can mutate freely. */
    cJSON *exp_copy = cJSON_Duplicate(e_result, 1);
    cJSON *act_copy = cJSON_Duplicate(actual, 1);

    /* Port substitution in expected.result (e.g. expected URLs reference {port}). */
    substitute_port_in_json(exp_copy, port);

    /* Resolve wildcard sentinels: "IGNORED" / "NON_NULL". */
    apply_wildcard_sentinels(exp_copy, act_copy);

    /* Apply path-based ignores. */
    const char **ig = NULL;
    size_t ign = extract_ignore_paths(exp, &ig);
    diff_strip_ignores(exp_copy, ig, ign);
    diff_strip_ignores(act_copy, ig, ign);
    free(ig);

    diff_report_t rep; diff_report_init(&rep);
    int result = diff_compare(exp_copy, act_copy, &rep);

    if (result == 0) report_pass(v, cfg);
    else             report_fail(v, extension_vector ? "extension-diff" : "execute-diff",
                                 &rep, cfg);

    diff_report_free(&rep);
    cJSON_Delete(exp_copy);
    cJSON_Delete(act_copy);
    cJSON_Delete(actual);
    adapter_result_free(&res);
    return result;
}

static int run_execute_vector(const vector_t *v, const runner_config_t *cfg) {
    return run_execute_vector_common(v, cfg, false);
}

static int run_extension_vector(const vector_t *v, const runner_config_t *cfg) {
    return run_execute_vector_common(v, cfg, true);
}

/* ── entrypoint ───────────────────────────────────────────────────── */

/* Sentinel returned by per-type runners when the vector was skipped (TLS
 * unavailable, cert missing, …). Distinct from 0 (pass) and 1 (fail) so
 * runner_run_all can account for it in the summary. */
#define RUNNER_SKIP 2

/* Return the first feature name in the vector's `requires` that also appears
 * in the cfg's omit list, or NULL. */
static const char *vector_omitted_by(const vector_t *v, const runner_config_t *cfg) {
    if (!v->requires || v->requires_n == 0) return NULL;
    if (!cfg->omit || cfg->omit_count == 0) return NULL;
    for (size_t i = 0; i < v->requires_n; i++) {
        for (size_t j = 0; j < cfg->omit_count; j++) {
            if (cfg->omit[j] && strcmp(v->requires[i], cfg->omit[j]) == 0)
                return v->requires[i];
        }
    }
    return NULL;
}

int runner_run_all(const vector_list_t *vectors, const runner_config_t *cfg, runner_summary_t *sum) {
    if (sum) memset(sum, 0, sizeof(*sum));

    for (size_t i = 0; i < vectors->n; i++) {
        const vector_t *v = &vectors->items[i];

        /* Spec §17: skip vectors whose `requires` intersects the omit set. */
        const char *omitted = vector_omitted_by(v, cfg);
        if (omitted) {
            char reason[128];
            snprintf(reason, sizeof(reason), "omitted: %s", omitted);
            report_skip(v, reason, cfg);
            if (sum) { sum->skipped++; sum->omitted++; sum->total++; }
            continue;
        }

        int r = 0;
        switch (v->type) {
        case VEC_TYPE_PARSE:
            r = run_parse_vector(v, cfg);
            break;
        case VEC_TYPE_VALIDATE:
            r = run_validate_vector(v, cfg);
            break;
        case VEC_TYPE_EXECUTE:
            r = run_execute_vector(v, cfg);
            break;
        case VEC_TYPE_EXTENSION:
            r = run_extension_vector(v, cfg);
            break;
        default:
            report_skip(v, "unknown type", cfg);
            if (sum) sum->skipped++;
            sum->total++;
            continue;
        }

        if (sum) sum->total++;
        if (r == 0)                     { if (sum) sum->passed++;  }
        else if (r == RUNNER_SKIP)      { if (sum) sum->skipped++; }
        else                            { if (sum) sum->failed++;  }
    }
    return (sum && sum->failed > 0) ? 1 : 0;
}
