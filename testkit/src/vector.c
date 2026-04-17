#define _POSIX_C_SOURCE 200809L

#include "vector.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ── file I/O ──────────────────────────────────────────────────────── */

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = 0;
    if (out_len) *out_len = n;
    return buf;
}

/* ── type parsing ──────────────────────────────────────────────────── */

static vector_type_t parse_type(const char *s) {
    if (!s) return VEC_TYPE_UNKNOWN;
    if (strcmp(s, "parse")     == 0) return VEC_TYPE_PARSE;
    if (strcmp(s, "validate")  == 0) return VEC_TYPE_VALIDATE;
    if (strcmp(s, "execute")   == 0) return VEC_TYPE_EXECUTE;
    if (strcmp(s, "extension") == 0) return VEC_TYPE_EXTENSION;
    return VEC_TYPE_UNKNOWN;
}

const char *vector_type_name(vector_type_t t) {
    switch (t) {
    case VEC_TYPE_PARSE:     return "parse";
    case VEC_TYPE_VALIDATE:  return "validate";
    case VEC_TYPE_EXECUTE:   return "execute";
    case VEC_TYPE_EXTENSION: return "extension";
    default:                 return "unknown";
    }
}

/* ── single-vector load ────────────────────────────────────────────── */

static int load_one(const char *path, const char *rel, vector_t *out) {
    memset(out, 0, sizeof(*out));

    size_t len = 0;
    char *text = read_file(path, &len);
    if (!text) return -1;

    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    if (!root) return -1;

    cJSON *id    = cJSON_GetObjectItem(root, "id");
    cJSON *desc  = cJSON_GetObjectItem(root, "description");
    cJSON *type  = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(id) || !cJSON_IsString(desc) || !cJSON_IsString(type)) {
        cJSON_Delete(root);
        return -1;
    }

    out->path        = strdup(path);
    out->rel         = strdup(rel ? rel : path);
    out->id          = strdup(id->valuestring);
    out->description = strdup(desc->valuestring);
    out->type        = parse_type(type->valuestring);
    out->root        = root;

    cJSON *req = cJSON_GetObjectItem(root, "requires");
    if (cJSON_IsArray(req)) {
        int n = cJSON_GetArraySize(req);
        if (n > 0) {
            out->requires = calloc((size_t)n + 1, sizeof(char *));
            if (out->requires) {
                int k = 0;
                for (int i = 0; i < n; i++) {
                    cJSON *s = cJSON_GetArrayItem(req, i);
                    if (cJSON_IsString(s)) out->requires[k++] = strdup(s->valuestring);
                }
                out->requires_n = (size_t)k;
            }
        }
    }
    return 0;
}

/* ── directory walk ────────────────────────────────────────────────── */

static void list_push(vector_list_t *l, vector_t v) {
    if (l->n + 1 > l->cap) {
        size_t nc = l->cap ? l->cap * 2 : 64;
        vector_t *ni = realloc(l->items, nc * sizeof(vector_t));
        if (!ni) return;
        l->items = ni; l->cap = nc;
    }
    l->items[l->n++] = v;
}

static int by_rel(const void *a, const void *b) {
    return strcmp(((const vector_t*)a)->rel, ((const vector_t*)b)->rel);
}

static int walk(
    const char *base,
    const char *root_len_path, /* base root for computing relative */
    const char *filter,
    vector_list_t *out
) {
    DIR *d = opendir(base);
    if (!d) return -1;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;

        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", base, e->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk(full, root_len_path, filter, out);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;

        size_t dn = strlen(e->d_name);
        if (dn < 6 || strcmp(e->d_name + dn - 5, ".json") != 0) continue;

        /* relative = full - root_len_path */
        size_t rp = strlen(root_len_path);
        const char *rel = full;
        if (strncmp(full, root_len_path, rp) == 0) {
            rel = full + rp;
            if (*rel == '/') rel++;
        }

        if (filter && filter[0] && !strstr(rel, filter) && !strstr(e->d_name, filter)) continue;

        vector_t v;
        if (load_one(full, rel, &v) == 0) list_push(out, v);
    }
    closedir(d);
    return 0;
}

int vector_load_dir(const char *root, const char *filter, vector_list_t *out) {
    out->items = NULL; out->n = 0; out->cap = 0;
    if (walk(root, root, filter, out) != 0) return -1;
    if (out->n) qsort(out->items, out->n, sizeof(vector_t), by_rel);
    return 0;
}

void vector_list_merge(vector_list_t *dst, vector_list_t *src) {
    for (size_t i = 0; i < src->n; i++) {
        list_push(dst, src->items[i]);
    }
    free(src->items);
    src->items = NULL;
    src->n = src->cap = 0;
    if (dst->n > 1)
        qsort(dst->items, dst->n, sizeof(vector_t), by_rel);
}

void vector_list_free(vector_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->n; i++) {
        free(list->items[i].path);
        free(list->items[i].rel);
        free(list->items[i].id);
        free(list->items[i].description);
        if (list->items[i].requires) {
            for (size_t k = 0; k < list->items[i].requires_n; k++)
                free(list->items[i].requires[k]);
            free(list->items[i].requires);
        }
        cJSON_Delete(list->items[i].root);
    }
    free(list->items);
    list->items = NULL; list->n = list->cap = 0;
}

const cJSON *vector_input(const vector_t *v)    { return cJSON_GetObjectItem(v->root, "input"); }
const cJSON *vector_expected(const vector_t *v) { return cJSON_GetObjectItem(v->root, "expected"); }

/* ── embedded vectors ──────────────────────────────────────────────── */

/* Provided by build/embedded_vectors.c when EMBED_VECTORS=1 is set at
 * compile time. Weak definitions keep the harness linkable without the
 * generated file for development builds. */
typedef struct { const char *rel; const char *json; size_t len; } embedded_vector_t;
extern const embedded_vector_t EMBEDDED_VECTORS[] __attribute__((weak));
extern const size_t EMBEDDED_VECTOR_COUNT __attribute__((weak));

int vector_load_embedded(const char *filter, vector_list_t *out) {
    out->items = NULL; out->n = 0; out->cap = 0;
    if (&EMBEDDED_VECTOR_COUNT == NULL || EMBEDDED_VECTOR_COUNT == 0) return -1;

    for (size_t i = 0; i < EMBEDDED_VECTOR_COUNT; i++) {
        const embedded_vector_t *ev = &EMBEDDED_VECTORS[i];
        if (filter && filter[0] && !strstr(ev->rel, filter)) continue;

        cJSON *root = cJSON_ParseWithLength(ev->json, ev->len);
        if (!root) continue;

        cJSON *id   = cJSON_GetObjectItem(root, "id");
        cJSON *desc = cJSON_GetObjectItem(root, "description");
        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (!cJSON_IsString(id) || !cJSON_IsString(desc) || !cJSON_IsString(type)) {
            cJSON_Delete(root); continue;
        }

        vector_t v = {0};
        v.path        = strdup(ev->rel);   /* no real filesystem path */
        v.rel         = strdup(ev->rel);
        v.id          = strdup(id->valuestring);
        v.description = strdup(desc->valuestring);
        v.type        = parse_type(type->valuestring);
        v.root        = root;
        cJSON *req = cJSON_GetObjectItem(root, "requires");
        if (cJSON_IsArray(req)) {
            int rn = cJSON_GetArraySize(req);
            if (rn > 0) {
                v.requires = calloc((size_t)rn + 1, sizeof(char *));
                if (v.requires) {
                    int k = 0;
                    for (int ri = 0; ri < rn; ri++) {
                        cJSON *s = cJSON_GetArrayItem(req, ri);
                        if (cJSON_IsString(s)) v.requires[k++] = strdup(s->valuestring);
                    }
                    v.requires_n = (size_t)k;
                }
            }
        }
        list_push(out, v);
    }
    if (out->n) qsort(out->items, out->n, sizeof(vector_t), by_rel);
    return 0;
}

const char *vector_category(const vector_t *v) {
    const char *sep = strchr(v->rel, '/');
    if (!sep) sep = strchr(v->rel, '\\');
    if (!sep) return NULL;
    static __thread char buf[128];
    size_t len = (size_t)(sep - v->rel);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, v->rel, len);
    buf[len] = 0;
    return buf;
}
