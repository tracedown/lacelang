#define _POSIX_C_SOURCE 200809L

#include "reporter_junit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char           *classname;
    char           *name;
    junit_outcome_t outcome;
    double          elapsed;
    char           *message;
} junit_case_t;

struct junit_reporter {
    char          *suite_name;
    junit_case_t  *cases;
    size_t         n, cap;
};

junit_reporter_t *junit_reporter_new(const char *suite_name) {
    junit_reporter_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->suite_name = strdup(suite_name ? suite_name : "lace-conformance");
    return r;
}

void junit_reporter_free(junit_reporter_t *r) {
    if (!r) return;
    for (size_t i = 0; i < r->n; i++) {
        free(r->cases[i].classname);
        free(r->cases[i].name);
        free(r->cases[i].message);
    }
    free(r->cases);
    free(r->suite_name);
    free(r);
}

void junit_reporter_add(
    junit_reporter_t *r,
    const char       *classname,
    const char       *test_name,
    junit_outcome_t   outcome,
    double            elapsed_seconds,
    const char       *message
) {
    if (!r) return;
    if (r->n + 1 > r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 32;
        junit_case_t *na = realloc(r->cases, nc * sizeof(*na));
        if (!na) return;
        r->cases = na;
        r->cap = nc;
    }
    junit_case_t *c = &r->cases[r->n++];
    memset(c, 0, sizeof(*c));
    c->classname = strdup(classname ? classname : "");
    c->name      = strdup(test_name ? test_name : "");
    c->outcome   = outcome;
    c->elapsed   = elapsed_seconds;
    c->message   = message ? strdup(message) : NULL;
}

/* XML-escape a string, writing the result to `out`. `out_cap` is total. */
static void xml_escape(const char *s, char *out, size_t out_cap) {
    if (!s) { out[0] = 0; return; }
    size_t w = 0;
    for (const char *p = s; *p && w + 8 < out_cap; p++) {
        switch (*p) {
        case '<':  w += (size_t)snprintf(out + w, out_cap - w, "&lt;");   break;
        case '>':  w += (size_t)snprintf(out + w, out_cap - w, "&gt;");   break;
        case '&':  w += (size_t)snprintf(out + w, out_cap - w, "&amp;");  break;
        case '"':  w += (size_t)snprintf(out + w, out_cap - w, "&quot;"); break;
        case '\'': w += (size_t)snprintf(out + w, out_cap - w, "&apos;"); break;
        default:
            if ((unsigned char)*p < 0x20 && *p != '\n' && *p != '\t') {
                /* skip unescapable control chars silently */
            } else {
                out[w++] = *p;
            }
        }
    }
    out[w] = 0;
}

int junit_reporter_write(const junit_reporter_t *r, const char *path) {
    if (!r || !path) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    size_t failures = 0, skipped = 0;
    double total_time = 0.0;
    for (size_t i = 0; i < r->n; i++) {
        if (r->cases[i].outcome == JUNIT_FAILED)  failures++;
        if (r->cases[i].outcome == JUNIT_SKIPPED) skipped++;
        total_time += r->cases[i].elapsed;
    }

    char suite_esc[256];
    xml_escape(r->suite_name, suite_esc, sizeof(suite_esc));

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuites>\n");
    fprintf(f, "  <testsuite name=\"%s\" tests=\"%zu\" failures=\"%zu\" "
               "skipped=\"%zu\" time=\"%.3f\">\n",
            suite_esc, r->n, failures, skipped, total_time);

    for (size_t i = 0; i < r->n; i++) {
        const junit_case_t *c = &r->cases[i];
        char cls[256], name[256], msg[2048];
        xml_escape(c->classname, cls,  sizeof(cls));
        xml_escape(c->name,      name, sizeof(name));
        xml_escape(c->message,   msg,  sizeof(msg));

        fprintf(f, "    <testcase classname=\"%s\" name=\"%s\" time=\"%.3f\"",
                cls, name, c->elapsed);
        switch (c->outcome) {
        case JUNIT_PASSED:
            fprintf(f, "/>\n");
            break;
        case JUNIT_SKIPPED:
            fprintf(f, ">\n      <skipped message=\"%s\"/>\n    </testcase>\n", msg);
            break;
        case JUNIT_FAILED:
            fprintf(f, ">\n      <failure message=\"%s\">%s</failure>\n    </testcase>\n",
                    msg, msg);
            break;
        }
    }

    fprintf(f, "  </testsuite>\n");
    fprintf(f, "</testsuites>\n");
    fclose(f);
    return 0;
}
