/*
 * vector — loading and representation of conformance test vectors.
 *
 * On disk, vectors are JSON files conforming to the schema at
 * lacelang/specs/schemas/conformance-vector.json. This module loads
 * them into cJSON trees and performs a minimal shape check.
 */

#ifndef VECTOR_H
#define VECTOR_H

#include "third_party/cjson/cJSON.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    VEC_TYPE_PARSE,
    VEC_TYPE_VALIDATE,
    VEC_TYPE_EXECUTE,
    VEC_TYPE_EXTENSION,
    VEC_TYPE_UNKNOWN
} vector_type_t;

typedef struct {
    char          *path;          /* absolute path to the .json file */
    char          *rel;           /* path relative to vectors root   */
    char          *id;            /* from JSON "id"                  */
    char          *description;   /* from JSON "description"         */
    vector_type_t  type;
    /* `requires` feature list (spec §17). Owned array of strings,
     * terminated by a NULL entry. Empty (or NULL) = no requirements. */
    char         **requires;
    size_t         requires_n;
    cJSON         *root;          /* owns; cJSON_Delete on free      */
} vector_t;

typedef struct {
    vector_t *items;
    size_t    n;
    size_t    cap;
} vector_list_t;

int  vector_load_dir(const char *root, const char *filter, vector_list_t *out);

/* Load vectors from the embedded compile-time snapshot. Returns -1 if no
 * embedded vectors were linked (see tools/embed_vectors.py + Makefile). */
int  vector_load_embedded(const char *filter, vector_list_t *out);

/* Merge all vectors from src into dst, then re-sort. src is emptied. */
void vector_list_merge(vector_list_t *dst, vector_list_t *src);

void vector_list_free(vector_list_t *list);

/* Category directory component of a vector's relative path, or NULL. */
const char *vector_category(const vector_t *v);

/* Returns the "input" or "expected" object sub-tree, or NULL. */
const cJSON *vector_input(const vector_t *v);
const cJSON *vector_expected(const vector_t *v);

/* String-type name for diagnostics. */
const char *vector_type_name(vector_type_t t);

#endif
