/*
 * manifest — minimal TOML subset parser + template expansion.
 *
 * The testkit's manifest consumes a small, well-defined subset of TOML:
 *   - `[section]` and `[section.subsection]` headers
 *   - `key = "string"` (basic strings, `\"` / `\\` / `\n` / `\t` escapes)
 *   - `key = integer`
 *   - `# comment` lines, trailing whitespace
 *
 * Arrays, dates, inline tables, heredocs, multi-line strings — all out of
 * scope. The executor-manifest format does not require them.
 */

#define _POSIX_C_SOURCE 200809L

#include "manifest.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── shared helpers ─────────────────────────────────────────────── */

static char *x_strdup(const char *s) { return s ? strdup(s) : NULL; }

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    return buf;
}

/* ── minimal TOML scanner ───────────────────────────────────────── */

typedef struct {
    char  *section;        /* e.g. "adapter" or "adapter.env" */
    char  *key;
    char  *value_str;      /* if string */
    long   value_int;
    /* if value is an array of strings (for [conformance].omit) */
    char **value_arr;
    size_t value_arr_n;
    int    kind;           /* 0=integer, 1=string, 2=string-array */
    int    valid;
} toml_pair_t;

static void ltrim(const char **p) { while (**p == ' ' || **p == '\t') (*p)++; }

static int parse_key(const char **p, char **out) {
    ltrim(p);
    const char *start = *p;
    while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '-' || **p == '.')) (*p)++;
    size_t n = (size_t)(*p - start);
    if (n == 0) return -1;
    *out = strndup(start, n);
    return 0;
}

static int parse_string(const char **p, char **out) {
    ltrim(p);
    if (**p != '"') return -1;
    (*p)++;
    size_t cap = 64, w = 0;
    char *buf = malloc(cap);
    if (!buf) return -1;
    while (**p && **p != '"') {
        if (w + 2 >= cap) {
            cap *= 2;
            char *r = realloc(buf, cap);
            if (!r) { free(buf); return -1; }
            buf = r;
        }
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
            case 'n':  buf[w++] = '\n'; break;
            case 't':  buf[w++] = '\t'; break;
            case 'r':  buf[w++] = '\r'; break;
            case '"':  buf[w++] = '"';  break;
            case '\\': buf[w++] = '\\'; break;
            case 0:    free(buf); return -1;
            default:   buf[w++] = **p;
            }
            (*p)++;
        } else {
            buf[w++] = *(*p)++;
        }
    }
    if (**p != '"') { free(buf); return -1; }
    (*p)++;
    buf[w] = 0;
    *out = buf;
    return 0;
}

static int parse_integer(const char **p, long *out) {
    ltrim(p);
    char *end = NULL;
    long v = strtol(*p, &end, 10);
    if (end == *p) return -1;
    *out = v;
    *p = end;
    return 0;
}

/* Parse one non-blank line. Returns 0 on success, -1 on error, 1 if the
 * line didn't contain a directive (blank / comment). `cur_section` is
 * updated on `[section]` headers. */
static int parse_line(const char *line, char **cur_section, toml_pair_t *out) {
    memset(out, 0, sizeof(*out));
    const char *p = line;
    ltrim(&p);
    if (*p == 0 || *p == '#' || *p == '\n' || *p == '\r') return 1;

    if (*p == '[') {
        p++;
        const char *start = p;
        while (*p && *p != ']') p++;
        if (*p != ']') return -1;
        size_t n = (size_t)(p - start);
        free(*cur_section);
        *cur_section = strndup(start, n);
        return 1;
    }

    char *key = NULL;
    if (parse_key(&p, &key) != 0) return -1;
    ltrim(&p);
    if (*p != '=') { free(key); return -1; }
    p++;
    ltrim(&p);
    out->section = x_strdup(*cur_section);
    out->key = key;
    if (*p == '"') {
        if (parse_string(&p, &out->value_str) != 0) { free(key); free(out->section); return -1; }
        out->kind = 1;
    } else if (*p == '[') {
        /* Single-line array of strings. Minimal parser — commas separated,
         * whitespace skipped between tokens, trailing comma optional. */
        p++;
        ltrim(&p);
        size_t cap = 4;
        out->value_arr = calloc(cap, sizeof(char *));
        if (!out->value_arr) { free(key); free(out->section); return -1; }
        while (*p && *p != ']') {
            char *s = NULL;
            if (parse_string(&p, &s) != 0) {
                for (size_t i = 0; i < out->value_arr_n; i++) free(out->value_arr[i]);
                free(out->value_arr); free(key); free(out->section); return -1;
            }
            if (out->value_arr_n + 1 >= cap) {
                cap *= 2;
                char **r = realloc(out->value_arr, cap * sizeof(char *));
                if (!r) { free(s); /* caller cleanup below */ break; }
                out->value_arr = r;
            }
            out->value_arr[out->value_arr_n++] = s;
            ltrim(&p);
            if (*p == ',') { p++; ltrim(&p); }
        }
        if (*p != ']') { free(key); free(out->section); return -1; }
        p++;
        out->kind = 2;
    } else if (isdigit((unsigned char)*p) || *p == '-' || *p == '+') {
        if (parse_integer(&p, &out->value_int) != 0) { free(key); free(out->section); return -1; }
        out->kind = 0;
    } else {
        free(key); free(out->section); return -1;
    }
    out->valid = 1;
    return 0;
}

/* ── manifest load ──────────────────────────────────────────────── */

static void set_string_field(char **dst, const char *v) {
    free(*dst);
    *dst = x_strdup(v);
}

static int section_eq(const char *s, const char *want) {
    return s && strcmp(s, want) == 0;
}

executor_manifest_t *manifest_load(const char *path) {
    char *text = slurp(path);
    if (!text) {
        fprintf(stderr, "manifest: could not read %s\n", path);
        return NULL;
    }

    executor_manifest_t *m = calloc(1, sizeof(*m));
    if (!m) { free(text); return NULL; }
    m->timeout_seconds = 0;

    char *cur_section = NULL;
    char *save = NULL;
    char *line = strtok_r(text, "\n", &save);
    int line_num = 0;
    int ok = 1;
    while (line) {
        line_num++;
        toml_pair_t p = {0};
        int r = parse_line(line, &cur_section, &p);
        if (r < 0) {
            fprintf(stderr, "manifest: parse error at %s:%d\n", path, line_num);
            ok = 0;
            break;
        }
        if (r == 0 && p.valid) {
            if (section_eq(p.section, "executor") && p.kind == 1) {
                if      (strcmp(p.key, "name")        == 0) set_string_field(&m->name, p.value_str);
                else if (strcmp(p.key, "version")     == 0) set_string_field(&m->version, p.value_str);
                else if (strcmp(p.key, "language")    == 0) set_string_field(&m->language, p.value_str);
                else if (strcmp(p.key, "conforms_to") == 0) set_string_field(&m->conforms_to, p.value_str);
            } else if (section_eq(p.section, "adapter")) {
                if (p.kind == 1) {
                    if      (strcmp(p.key, "parse")    == 0) set_string_field(&m->parse_template, p.value_str);
                    else if (strcmp(p.key, "validate") == 0) set_string_field(&m->validate_template, p.value_str);
                    else if (strcmp(p.key, "run")      == 0) set_string_field(&m->run_template, p.value_str);
                } else if (p.kind == 0 && strcmp(p.key, "timeout_seconds") == 0) {
                    m->timeout_seconds = (int)p.value_int;
                }
            } else if (section_eq(p.section, "adapter.env") && p.kind == 1) {
                char **ne = realloc(m->env_pairs, (m->env_count + 1) * 2 * sizeof(char *));
                if (!ne) { ok = 0; free(p.value_str); free(p.key); free(p.section); break; }
                m->env_pairs = ne;
                m->env_pairs[m->env_count * 2]     = x_strdup(p.key);
                m->env_pairs[m->env_count * 2 + 1] = x_strdup(p.value_str);
                m->env_count++;
            } else if (section_eq(p.section, "conformance") && p.kind == 2
                       && strcmp(p.key, "omit") == 0) {
                m->omit = calloc(p.value_arr_n + 1, sizeof(char *));
                if (m->omit) {
                    for (size_t i = 0; i < p.value_arr_n; i++)
                        m->omit[i] = x_strdup(p.value_arr[i]);
                    m->omit_count = p.value_arr_n;
                }
            }
            free(p.value_str);
            if (p.value_arr) {
                for (size_t i = 0; i < p.value_arr_n; i++) free(p.value_arr[i]);
                free(p.value_arr);
            }
            free(p.key);
            free(p.section);
        }
        line = strtok_r(NULL, "\n", &save);
    }
    free(cur_section);
    free(text);

    if (ok && (!m->name || !m->parse_template || !m->validate_template || !m->run_template)) {
        fprintf(stderr, "manifest: missing required fields (executor.name / adapter.parse / validate / run)\n");
        ok = 0;
    }
    if (!ok) { manifest_free(m); return NULL; }
    return m;
}

void manifest_free(executor_manifest_t *m) {
    if (!m) return;
    free(m->name); free(m->version); free(m->language); free(m->conforms_to);
    free(m->parse_template); free(m->validate_template); free(m->run_template);
    if (m->env_pairs) {
        for (size_t i = 0; i < m->env_count * 2; i++) free(m->env_pairs[i]);
        free(m->env_pairs);
    }
    if (m->omit) {
        for (size_t i = 0; i < m->omit_count; i++) free(m->omit[i]);
        free(m->omit);
    }
    free(m);
}

/* ── template expansion ─────────────────────────────────────────── */

static const char *placeholder_value(
    const char *name, size_t len,
    const char *script, const char *vars, const char *vars_list,
    const char *context, const char *prev, const char *config
) {
    if (len == 6 && strncmp(name, "script", 6)   == 0) return script    ? script    : "";
    if (len == 4 && strncmp(name, "vars",   4)   == 0) return vars      ? vars      : "";
    if (len == 9 && strncmp(name, "vars_list", 9)== 0) return vars_list ? vars_list : "";
    if (len == 7 && strncmp(name, "context", 7)  == 0) return context   ? context   : "";
    if (len == 4 && strncmp(name, "prev",   4)   == 0) return prev      ? prev      : "";
    if (len == 6 && strncmp(name, "config", 6)   == 0) return config    ? config    : "";
    return NULL;   /* unknown placeholder → preserve literally */
}

/* First pass — compute the expanded length; second pass — materialise. */
static char *expand_to_string(
    const char *tpl,
    const char *script, const char *vars, const char *vars_list,
    const char *context, const char *prev, const char *config
) {
    size_t out_cap = strlen(tpl) + 256, w = 0;
    char  *out = malloc(out_cap);
    if (!out) return NULL;
    const char *p = tpl;
    while (*p) {
        if (*p == '{') {
            const char *end = strchr(p + 1, '}');
            if (end) {
                const char *v = placeholder_value(p + 1, (size_t)(end - p - 1),
                                                  script, vars, vars_list,
                                                  context, prev, config);
                if (v) {
                    size_t n = strlen(v);
                    while (w + n + 1 >= out_cap) {
                        out_cap *= 2;
                        char *r = realloc(out, out_cap);
                        if (!r) { free(out); return NULL; }
                        out = r;
                    }
                    memcpy(out + w, v, n);
                    w += n;
                    p = end + 1;
                    continue;
                }
            }
        }
        if (w + 2 >= out_cap) {
            out_cap *= 2;
            char *r = realloc(out, out_cap);
            if (!r) { free(out); return NULL; }
            out = r;
        }
        out[w++] = *p++;
    }
    out[w] = 0;
    return out;
}

/* Split a string on whitespace into argv. Quoted segments (single or double
 * quotes) are kept as one token; escape sequences inside quotes are not
 * interpreted — users of manifests should avoid embedding quoted shell
 * specials in templates. */
char **manifest_expand_template(
    const char *template_str,
    const char *script,
    const char *vars,
    const char *vars_list,
    const char *context,
    const char *prev,
    const char *config
) {
    if (!template_str) return NULL;
    char *flat = expand_to_string(template_str, script, vars, vars_list,
                                  context, prev, config);
    if (!flat) return NULL;

    size_t cap = 16, n = 0;
    char **argv = calloc(cap, sizeof(char *));
    if (!argv) { free(flat); return NULL; }

    char *p = flat;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) break;
        char quote = 0;
        if (*p == '"' || *p == '\'') { quote = *p; p++; }
        char *tok = p;
        if (quote) {
            while (*p && *p != quote) p++;
            if (*p == quote) { *p = 0; p++; }
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) { *p = 0; p++; }
        }
        if (n + 2 >= cap) {
            cap *= 2;
            char **r = realloc(argv, cap * sizeof(char *));
            if (!r) { free(flat); free(argv); return NULL; }
            argv = r;
        }
        argv[n++] = tok;
    }
    argv[n] = NULL;
    /* argv[0] is the owning pointer of the flat buffer; the caller frees via
     * manifest_argv_free below. */
    if (n == 0) { free(flat); free(argv); return NULL; }
    return argv;
}

void manifest_argv_free(char **argv) {
    if (!argv) return;
    /* All tokens live inside a single malloc'd buffer whose address is
     * argv[0] — freeing argv[0] frees every token. */
    if (argv[0]) free(argv[0]);
    free(argv);
}
