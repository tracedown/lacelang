/*
 * diff — JSON deep equality implementation.
 *
 * Ignore path syntax:
 *   segment        -> object field
 *   segment[N]     -> array index
 *   segment[*]     -> any array index (wildcard)
 *   segment.sub    -> nested field
 *
 * Implementation approach: for each ignore-path, walk the tree following
 * the segments, and at the final step delete the named field (or for
 * array-index paths, delete the containing field on the matched item —
 * but since we typically ignore leaf fields, the final step is always
 * a field deletion).
 */

#define _POSIX_C_SOURCE 200809L

#include "diff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── default ignores applied to every comparison ─────────────────── */

static const char *const DEFAULT_IGNORES[] = {
    "startedAt",
    "endedAt",
    "elapsedMs",
    "calls[*].startedAt",
    "calls[*].endedAt",
    "calls[*].request.bodyPath",
    "calls[*].response.bodyPath",
    "calls[*].request.headers.User-Agent",
    "calls[*].response.headers.content-length",
    "calls[*].response.headers.connection",
    /* Connection metadata (spec §3.4.1 / §3.4.2): varies per run / environment.
       Vectors that want to assert on specific DNS or TLS fields declare an
       explicit test for them; most vectors don't care and silently ignore. */
    "calls[*].response.dns",
    "calls[*].response.tls",
    NULL
};

/* ── report management ───────────────────────────────────────────── */

void diff_report_init(diff_report_t *r) { r->items = NULL; r->n = 0; r->cap = 0; }

void diff_report_free(diff_report_t *r) {
    if (!r) return;
    for (size_t i = 0; i < r->n; i++) { free(r->items[i].path); free(r->items[i].detail); }
    free(r->items);
    r->items = NULL; r->n = r->cap = 0;
}

static void report_add(diff_report_t *r, const char *path, const char *detail) {
    if (r->n + 1 > r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 16;
        diff_mismatch_t *ni = realloc(r->items, nc * sizeof(*ni));
        if (!ni) return;
        r->items = ni; r->cap = nc;
    }
    r->items[r->n].path   = strdup(path);
    r->items[r->n].detail = strdup(detail);
    r->n++;
}

void diff_report_print(const diff_report_t *r, const char *indent) {
    if (!r || !indent) return;
    for (size_t i = 0; i < r->n; i++) {
        fprintf(stdout, "%s%s: %s\n", indent, r->items[i].path, r->items[i].detail);
    }
}

/* ── path tokenization ───────────────────────────────────────────── */

typedef enum { SEG_FIELD, SEG_INDEX, SEG_WILDCARD } seg_kind_t;

typedef struct {
    seg_kind_t kind;
    char      *name;   /* for SEG_FIELD */
    int        index;  /* for SEG_INDEX */
} seg_t;

typedef struct {
    seg_t *segs;
    size_t n;
} path_t;

static void path_free(path_t *p) {
    for (size_t i = 0; i < p->n; i++) free(p->segs[i].name);
    free(p->segs);
    p->segs = NULL; p->n = 0;
}

/* Parse "a.b[0].c[*]" into segments. */
static int path_parse(const char *src, path_t *out) {
    out->segs = NULL; out->n = 0;
    size_t cap = 0;
    const char *p = src;

    while (*p) {
        /* Ensure capacity */
        if (out->n + 1 > cap) {
            cap = cap ? cap * 2 : 8;
            seg_t *ns = realloc(out->segs, cap * sizeof(seg_t));
            if (!ns) { path_free(out); return -1; }
            out->segs = ns;
        }

        if (*p == '.') { p++; continue; }

        if (*p == '[') {
            p++;
            if (*p == '*' && *(p+1) == ']') {
                out->segs[out->n].kind = SEG_WILDCARD;
                out->segs[out->n].name = NULL;
                out->segs[out->n].index = -1;
                out->n++;
                p += 2;
            } else {
                char *end;
                long idx = strtol(p, &end, 10);
                if (end == p || *end != ']') { path_free(out); return -1; }
                out->segs[out->n].kind  = SEG_INDEX;
                out->segs[out->n].name  = NULL;
                out->segs[out->n].index = (int)idx;
                out->n++;
                p = end + 1;
            }
            continue;
        }

        /* field name: [a-zA-Z_0-9]+ */
        const char *start = p;
        while (*p && *p != '.' && *p != '[') p++;
        size_t len = (size_t)(p - start);
        if (len == 0) { path_free(out); return -1; }
        out->segs[out->n].kind  = SEG_FIELD;
        out->segs[out->n].name  = strndup(start, len);
        out->segs[out->n].index = -1;
        out->n++;
    }
    return 0;
}

/* ── tree-walk for ignore stripping ──────────────────────────────── */

/* Recursive walk: at seg index si, if final segment is a field, delete
 * that field on the current node; for array segments, recurse into
 * matching element(s). */
static void strip_walk(cJSON *node, const seg_t *segs, size_t n, size_t si) {
    if (!node) return;
    if (si >= n) return;

    const seg_t *s = &segs[si];
    bool is_last = (si == n - 1);

    if (s->kind == SEG_FIELD) {
        if (!cJSON_IsObject(node)) return;
        if (is_last) {
            cJSON_DeleteItemFromObject(node, s->name);
            return;
        }
        cJSON *child = cJSON_GetObjectItem(node, s->name);
        strip_walk(child, segs, n, si + 1);
        return;
    }

    if (s->kind == SEG_INDEX) {
        if (!cJSON_IsArray(node)) return;
        cJSON *item = cJSON_GetArrayItem(node, s->index);
        if (!item) return;
        if (is_last) {
            cJSON_DeleteItemFromArray(node, s->index);
            return;
        }
        strip_walk(item, segs, n, si + 1);
        return;
    }

    if (s->kind == SEG_WILDCARD) {
        if (!cJSON_IsArray(node)) return;
        int sz = cJSON_GetArraySize(node);
        for (int i = 0; i < sz; i++) {
            cJSON *item = cJSON_GetArrayItem(node, i);
            if (is_last) {
                /* Wildcard can't be "the last" in isolation — it would mean
                 * "delete every array element" which our vectors don't use.
                 * Treat as no-op. */
                continue;
            }
            strip_walk(item, segs, n, si + 1);
        }
        return;
    }
}

static void strip_path(cJSON *root, const char *raw) {
    path_t p = {0};
    if (path_parse(raw, &p) != 0) return;
    strip_walk(root, p.segs, p.n, 0);
    path_free(&p);
}

void diff_strip_ignores(cJSON *node, const char *const *extra, size_t extra_n) {
    for (size_t i = 0; DEFAULT_IGNORES[i]; i++) strip_path(node, DEFAULT_IGNORES[i]);
    for (size_t i = 0; i < extra_n; i++) if (extra[i]) strip_path(node, extra[i]);
}

/* ── deep equality ───────────────────────────────────────────────── */

static void compare_node(
    const cJSON *exp,
    const cJSON *act,
    diff_report_t *r,
    char *path_buf,
    size_t path_cap
);

static void path_push_field(char *buf, size_t cap, const char *field) {
    size_t len = strlen(buf);
    if (len > 0) snprintf(buf + len, cap - len, ".%s", field);
    else         snprintf(buf,       cap,       "%s", field);
}

static void path_push_index(char *buf, size_t cap, int idx) {
    size_t len = strlen(buf);
    snprintf(buf + len, cap - len, "[%d]", idx);
}

static int path_mark(char *buf) { return (int)strlen(buf); }
static void path_restore(char *buf, int mark) { buf[mark] = 0; }

static void compare_node(
    const cJSON *exp,
    const cJSON *act,
    diff_report_t *r,
    char *pb,
    size_t pc
) {
    const char *path = pb[0] ? pb : "(root)";

    if ((exp == NULL) != (act == NULL)) {
        report_add(r, path, "one side is missing");
        return;
    }
    if (!exp && !act) return;

    int et = exp->type & 0xff, at = act->type & 0xff;
    if (et != at) {
        char detail[256];
        snprintf(detail, sizeof(detail), "type mismatch (expected %d, got %d)", et, at);
        report_add(r, path, detail);
        return;
    }

    if (cJSON_IsNull(exp)) return;
    if (cJSON_IsBool(exp)) {
        if (cJSON_IsTrue(exp) != cJSON_IsTrue(act)) report_add(r, path, "bool mismatch");
        return;
    }
    if (cJSON_IsNumber(exp)) {
        if (exp->valuedouble != act->valuedouble) {
            char detail[128];
            snprintf(detail, sizeof(detail), "number mismatch (expected %g, got %g)",
                     exp->valuedouble, act->valuedouble);
            report_add(r, path, detail);
        }
        return;
    }
    if (cJSON_IsString(exp)) {
        if (strcmp(exp->valuestring, act->valuestring) != 0) {
            char detail[512];
            snprintf(detail, sizeof(detail), "string mismatch (expected %.80s%s, got %.80s%s)",
                     exp->valuestring, strlen(exp->valuestring) > 80 ? "..." : "",
                     act->valuestring, strlen(act->valuestring) > 80 ? "..." : "");
            report_add(r, path, detail);
        }
        return;
    }
    if (cJSON_IsArray(exp)) {
        int en = cJSON_GetArraySize(exp), an = cJSON_GetArraySize(act);
        if (en != an) {
            char detail[128];
            snprintf(detail, sizeof(detail), "array length mismatch (expected %d, got %d)", en, an);
            report_add(r, path, detail);
            return;
        }
        for (int i = 0; i < en; i++) {
            int m = path_mark(pb);
            path_push_index(pb, pc, i);
            compare_node(cJSON_GetArrayItem(exp, i), cJSON_GetArrayItem(act, i), r, pb, pc);
            path_restore(pb, m);
        }
        return;
    }
    if (cJSON_IsObject(exp)) {
        /* check expected keys are present and equal */
        cJSON *echild = NULL;
        cJSON_ArrayForEach(echild, exp) {
            int m = path_mark(pb);
            path_push_field(pb, pc, echild->string);
            cJSON *achild = cJSON_GetObjectItem(act, echild->string);
            if (!achild) {
                report_add(r, pb, "key missing in actual");
            } else {
                compare_node(echild, achild, r, pb, pc);
            }
            path_restore(pb, m);
        }
        /* check for extra keys in actual */
        cJSON *achild = NULL;
        cJSON_ArrayForEach(achild, act) {
            if (!cJSON_GetObjectItem(exp, achild->string)) {
                int m = path_mark(pb);
                path_push_field(pb, pc, achild->string);
                report_add(r, pb, "unexpected key in actual");
                path_restore(pb, m);
            }
        }
        return;
    }
}

int diff_compare(const cJSON *expected, const cJSON *actual, diff_report_t *report) {
    char path_buf[4096] = {0};
    compare_node(expected, actual, report, path_buf, sizeof(path_buf));
    return report->n ? 1 : 0;
}
