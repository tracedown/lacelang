/*
 * lace-conformance — canonical conformance test harness for Lace executors.
 *
 * Capabilities:
 *   - CLI arg parsing (`-c <cmd>` OR `-m <manifest.toml>`)
 *   - Default-CLI adapter (standard parse/validate/run subcommand contract)
 *   - Manifest adapter (TOML templates with {script}/{vars}/{prev}/... placeholders)
 *   - Vector loading (recursive directory walk, filter by substring)
 *   - Runners for parse / validate / execute / extension vector types
 *   - Bundled HTTP mock server for execute vectors (threaded, sticky redirects,
 *     delay_ms, timeout outcome)
 *   - JSON diff engine with path ignores, IGNORED / NON_NULL sentinels, {port}
 *     substitution across script + variables + expected tree
 *   - Text, TAP, and JUnit XML reporters
 */

#include "manifest.h"
#include "reporter_junit.h"
#include "runner.h"
#include "vector.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

/* Injected by Makefile from ../../VERSION; fallback for standalone builds. */
#ifndef LACE_CONFORMANCE_VERSION
#define LACE_CONFORMANCE_VERSION "0.9.0"
#endif

typedef struct {
    const char       *executor_cmd;
    const char       *manifest_path;
    const char       *vectors_dir;
    const char       *extensions_dir;
    const char       *filter;
    const char       *output_path;
    const char       *certs_dir;
    const char       *omit_csv;          /* `--omit extensions,actions` */
    report_format_t   report;
    int               timeout_seconds;
    int               show_help;
    int               show_version;
    int               generate_certs;
} cli_args_t;

static void print_help(void) {
    fputs(
        "lace-conformance — canonical conformance harness for Lace executors\n"
        "\n"
        "Usage:\n"
        "  lace-conformance -c \"<cmd>\" [options]\n"
        "  lace-conformance -m <manifest.toml> [options]\n"
        "\n"
        "Executor selection (one required):\n"
        "  -c <cmd>          Executor command. The testkit invokes\n"
        "                    `<cmd> parse|validate|run <script> ...` with the\n"
        "                    standard subcommand contract.\n"
        "  -m <path>         Executor manifest. TOML file with per-subcommand\n"
        "                    templates — see executor-manifest.json schema.\n"
        "\n"
        "Vector selection:\n"
        "  --vectors <dir>   Root directory of vectors (default: ./vectors or\n"
        "                    $LACE_CONFORMANCE_VECTORS env var).\n"
        "  --extensions-dir <dir>\n"
        "                    Extensions directory containing extension-owned\n"
        "                    vectors (default: ../../extensions relative to\n"
        "                    vectors dir, or $LACE_CONFORMANCE_EXTENSIONS).\n"
        "                    Symlinked into the vectors tree during the run.\n"
        "  --filter <s>      Run only vectors whose id or rel-path contains <s>.\n"
        "\n"
        "Execution:\n"
        "  --timeout <sec>   Per-subprocess timeout in seconds (default 30).\n"
        "\n"
        "Output:\n"
        "  --report <fmt>    text (default), tap, or junit.\n"
        "  --output <path>   Write JUnit XML to <path> (default: output/\n"
        "                    lace-conformance.xml).\n"
        "\n"
        "                    All generated files (JUnit XML, body storage)\n"
        "                    are placed under output/ in the working directory.\n"
        "\n"
        "Conformance level (spec §17):\n"
        "  --omit <list>     Comma-separated features the executor does NOT\n"
        "                    implement. Valid: extensions, actions. Overrides\n"
        "                    the manifest's [conformance].omit. Vectors that\n"
        "                    require an omitted feature are skipped.\n"
        "\n"
        "TLS / HTTPS mock:\n"
        "  --certs-dir <dir> Override TLS cert directory. By default, certs\n"
        "                    are auto-generated to a temp dir and cleaned up\n"
        "                    after the run. Requires openssl on PATH.\n"
        "  --generate-certs  Populate --certs-dir with test certs and exit.\n"
        "                    Useful for pre-generating certs in CI.\n"
        "\n"
        "Other:\n"
        "  --version         Print version and exit.\n"
        "  --help            This message.\n"
        "\n"
        "Exit codes:\n"
        "  0  All runnable vectors passed.\n"
        "  1  At least one vector failed.\n"
        "  2  Fatal error (no executor specified, vectors dir unreadable, ...).\n",
        stdout
    );
}

static int parse_args(int argc, char **argv, cli_args_t *a) {
    memset(a, 0, sizeof(*a));
    a->report = REPORT_TEXT;
    a->timeout_seconds = 30;

    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];
        if (!strcmp(s, "-h") || !strcmp(s, "--help"))    { a->show_help = 1; }
        else if (!strcmp(s, "--version"))                { a->show_version = 1; }
        else if (!strcmp(s, "-c") && i + 1 < argc)       { a->executor_cmd = argv[++i]; }
        else if (!strcmp(s, "-m") && i + 1 < argc)       { a->manifest_path = argv[++i]; }
        else if (!strcmp(s, "--vectors") && i + 1 < argc){ a->vectors_dir = argv[++i]; }
        else if (!strcmp(s, "--extensions-dir") && i + 1 < argc){ a->extensions_dir = argv[++i]; }
        else if (!strcmp(s, "--filter") && i + 1 < argc) { a->filter = argv[++i]; }
        else if (!strcmp(s, "--timeout") && i + 1 < argc){ a->timeout_seconds = atoi(argv[++i]); }
        else if (!strcmp(s, "--report") && i + 1 < argc) {
            const char *f = argv[++i];
            if      (!strcmp(f, "text"))  a->report = REPORT_TEXT;
            else if (!strcmp(f, "tap"))   a->report = REPORT_TAP;
            else if (!strcmp(f, "junit")) a->report = REPORT_JUNIT;
            else { fprintf(stderr, "unknown report format: %s\n", f); return -1; }
        }
        else if (!strcmp(s, "--output") && i + 1 < argc) {
            a->output_path = argv[++i];
            a->report = REPORT_JUNIT;
        }
        else if (!strcmp(s, "--certs-dir") && i + 1 < argc) { a->certs_dir = argv[++i]; }
        else if (!strcmp(s, "--generate-certs"))             { a->generate_certs = 1; }
        else if (!strcmp(s, "--omit")       && i + 1 < argc) { a->omit_csv = argv[++i]; }
        else { fprintf(stderr, "unknown argument: %s\n", s); return -1; }
    }
    return 0;
}

/* Locate the vectors directory. Preference:
 *   1. --vectors arg
 *   2. LACE_CONFORMANCE_VECTORS env var
 *   3. ./vectors (relative to cwd)
 *   4. ../vectors (relative to binary dir — common when running from build/)
 */
static const char *resolve_vectors_dir(const cli_args_t *a, char out[4096]) {
    if (a->vectors_dir) { snprintf(out, 4096, "%s", a->vectors_dir); return out; }
    const char *env = getenv("LACE_CONFORMANCE_VECTORS");
    if (env && env[0]) { snprintf(out, 4096, "%s", env); return out; }

    struct stat st;
    if (stat("./vectors", &st) == 0 && S_ISDIR(st.st_mode)) { snprintf(out, 4096, "./vectors"); return out; }
    if (stat("../vectors", &st) == 0 && S_ISDIR(st.st_mode)) { snprintf(out, 4096, "../vectors"); return out; }
    return NULL;
}

/* Locate an existing TLS certs directory. Preference:
 *   1. --certs-dir arg
 *   2. LACE_CONFORMANCE_CERTS env var
 *   3. /usr/share/lacelang-testkit/certs (package install location)
 *   4. ./certs (relative to cwd — useful during dev)
 * Returns NULL when nothing is found; caller then auto-generates.
 */
static const char *resolve_certs_dir(const cli_args_t *a, char out[4096]) {
    if (a->certs_dir) { snprintf(out, 4096, "%s", a->certs_dir); return out; }
    const char *env = getenv("LACE_CONFORMANCE_CERTS");
    if (env && env[0]) { snprintf(out, 4096, "%s", env); return out; }

    struct stat st;
    const char *candidates[] = {
        "/usr/share/lacelang-testkit/certs",
        "/usr/local/share/lacelang-testkit/certs",
        "./certs",
        "../certs",
        NULL,
    };
    for (int i = 0; candidates[i]; i++) {
        if (stat(candidates[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(out, 4096, "%s", candidates[i]);
            return out;
        }
    }
    return NULL;
}

/* Locate the generate-certs.sh script. Preference:
 *   1. /usr/share/lacelang-testkit/generate-certs.sh (package install)
 *   2. <certs_dir>/generate-certs.sh (source tree — certs/ ships the script)
 *   3. ./certs/generate-certs.sh  (dev: running from testkit root)
 *   4. ../certs/generate-certs.sh (dev: running from src/)
 */
static const char *resolve_generate_script(const char *certs_dir, char out[4096]) {
    struct stat st;
    char tryp[4096];

    const char *pkg = "/usr/share/lacelang-testkit/generate-certs.sh";
    if (stat(pkg, &st) == 0) { snprintf(out, 4096, "%s", pkg); return out; }

    if (certs_dir) {
        snprintf(tryp, sizeof(tryp), "%s/generate-certs.sh", certs_dir);
        if (stat(tryp, &st) == 0) { snprintf(out, 4096, "%s", tryp); return out; }
    }
    const char *candidates[] = {
        "./certs/generate-certs.sh",
        "../certs/generate-certs.sh",
        NULL,
    };
    for (int i = 0; candidates[i]; i++) {
        if (stat(candidates[i], &st) == 0) {
            snprintf(out, 4096, "%s", candidates[i]);
            return out;
        }
    }
    return NULL;
}

/* Locate the extensions directory for extension-owned vectors.
 *   1. --extensions-dir arg
 *   2. LACE_CONFORMANCE_EXTENSIONS env var
 *   3. ../../extensions relative to the vectors dir (standard repo layout:
 *      vectors is testkit/vectors, extensions is lacelang/extensions)
 */
static const char *resolve_extensions_dir(
    const cli_args_t *a, const char *vdir, char out[4096]
) {
    if (a->extensions_dir) { snprintf(out, 4096, "%s", a->extensions_dir); return out; }
    const char *env = getenv("LACE_CONFORMANCE_EXTENSIONS");
    if (env && env[0]) { snprintf(out, 4096, "%s", env); return out; }

    if (vdir) {
        struct stat st;
        char try_path[4096];
        snprintf(try_path, sizeof(try_path), "%s/../../extensions", vdir);
        if (stat(try_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Resolve to a clean absolute path to avoid symlink confusion. */
            if (realpath(try_path, out)) return out;
            /* realpath failed — fall through to use as-is. */
            snprintf(out, 4096, "%s", try_path);
            return out;
        }
    }
    return NULL;
}

/* Create a symlink at `vectors/_extensions` → `extensions_dir`.
 * Returns 0 on success, -1 on failure. Removes a stale symlink first. */
static int create_extensions_symlink(const char *vdir, const char *ext_dir,
                                     char link_path[4096]) {
    snprintf(link_path, 4096, "%s/_extensions", vdir);
    /* Remove stale symlink or directory if present. */
    struct stat lst;
    if (lstat(link_path, &lst) == 0) {
        if (S_ISLNK(lst.st_mode)) {
            unlink(link_path);
        } else {
            /* Not a symlink — refuse to clobber. */
            fprintf(stderr, "Error: %s exists and is not a symlink; "
                    "refusing to overwrite.\n", link_path);
            return -1;
        }
    }
    if (symlink(ext_dir, link_path) != 0) {
        fprintf(stderr, "Error: symlink(%s, %s): %s\n",
                ext_dir, link_path, strerror(errno));
        return -1;
    }
    return 0;
}

static void remove_extensions_symlink(const char *link_path) {
    struct stat lst;
    if (lstat(link_path, &lst) == 0 && S_ISLNK(lst.st_mode)) {
        unlink(link_path);
    }
}

/* Check if a string appears in a NULL-terminated array. */
static bool omit_contains(const char *const *arr, size_t count,
                           const char *feature) {
    for (size_t i = 0; i < count; i++) {
        if (arr[i] && strcmp(arr[i], feature) == 0) return true;
    }
    return false;
}

/* Recursively remove a directory tree. Best-effort. */
static void rmtree_main(const char *path) {
    DIR *d = opendir(path);
    if (!d) { rmdir(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode)) {
            rmtree_main(child);
        } else {
            unlink(child);
        }
    }
    closedir(d);
    rmdir(path);
}

/* Run generate-certs.sh as a subprocess (fork+exec), writing certs to
 * `out_dir`. Returns 0 on success, non-zero on failure. */
static int run_generate_certs(const char *script_path, const char *out_dir) {
    fprintf(stderr, "── generating TLS test certs → %s\n", out_dir);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* Redirect stdout/stderr to /dev/null for clean output —
         * generate-certs.sh writes progress to stderr. Keep stderr. */
        execl("/bin/sh", "sh", script_path, out_dir, (char *)NULL);
        fprintf(stderr, "exec sh %s: %s\n", script_path, strerror(errno));
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) != pid) return -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Error: generate-certs.sh failed (exit %d).\n"
                        "       Ensure openssl is installed on PATH.\n",
                WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return -1;
    }
    return 0;
}

static int do_generate_certs(const cli_args_t *a) {
    const char *certs = a->certs_dir
        ? a->certs_dir
        : (getenv("LACE_CONFORMANCE_CERTS") && getenv("LACE_CONFORMANCE_CERTS")[0]
            ? getenv("LACE_CONFORMANCE_CERTS")
            : (geteuid() == 0 ? "/usr/share/lacelang-testkit/certs" : "./certs"));

    char script[4096];
    const char *s = resolve_generate_script(certs, script);
    if (!s) {
        fputs("Error: generate-certs.sh not found. Reinstall lacelang-testkit or\n"
              "       run from the source tree (testkit/certs/generate-certs.sh).\n", stderr);
        return 2;
    }
    return run_generate_certs(s, certs) == 0 ? 0 : 2;
}

/* Auto-generate TLS certs into a temp directory. Returns 0 on success and
 * fills `out_dir` with the temp path (caller must rmtree on exit). */
static int auto_generate_certs(char out_dir[4096]) {
    snprintf(out_dir, 4096, "/tmp/lace-certs-XXXXXX");
    if (!mkdtemp(out_dir)) {
        fprintf(stderr, "Error: mkdtemp for TLS certs: %s\n", strerror(errno));
        return -1;
    }
    char script[4096];
    /* Pass NULL as certs_dir hint — resolve_generate_script will search
     * the standard candidate paths for the script. */
    const char *s = resolve_generate_script(NULL, script);
    if (!s) {
        fputs("Error: generate-certs.sh not found. Cannot auto-generate TLS certs.\n"
              "       Reinstall lacelang-testkit or run from the source tree.\n", stderr);
        rmtree_main(out_dir);
        return -1;
    }
    if (run_generate_certs(s, out_dir) != 0) {
        rmtree_main(out_dir);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    cli_args_t a;
    if (parse_args(argc, argv, &a) != 0) {
        fputs("Run 'lace-conformance --help' for usage.\n", stderr);
        return 2;
    }
    if (a.show_help)      { print_help(); return 0; }
    if (a.show_version)   { puts("lace-conformance " LACE_CONFORMANCE_VERSION); return 0; }
    if (a.generate_certs) { return do_generate_certs(&a); }

    if (!a.executor_cmd && !a.manifest_path) {
        fputs("Error: must supply -c <cmd> or -m <manifest>.\n", stderr);
        return 2;
    }
    executor_manifest_t *manifest = NULL;
    if (a.manifest_path) {
        manifest = manifest_load(a.manifest_path);
        if (!manifest) return 2;
    }

    char vdir_buf[4096];
    const char *vdir = resolve_vectors_dir(&a, vdir_buf);

    vector_list_t vectors;
    if (vdir) {
        if (a.report == REPORT_TEXT) {
            fprintf(stderr, "── lace-conformance — vectors: %s", vdir);
            if (a.filter) fprintf(stderr, "  filter: %s", a.filter);
            fputc('\n', stderr);
        }
        if (vector_load_dir(vdir, a.filter, &vectors) != 0) {
            fprintf(stderr, "Error: could not read vectors from %s\n", vdir);
            return 2;
        }
    } else {
        if (vector_load_embedded(a.filter, &vectors) != 0) {
            fputs("Error: no vectors directory located AND no vectors embedded in this build. "
                  "Pass --vectors <path>, set $LACE_CONFORMANCE_VECTORS, or rebuild with "
                  "EMBED_VECTORS=1.\n", stderr);
            return 2;
        }
        if (a.report == REPORT_TEXT) {
            fprintf(stderr, "── lace-conformance — vectors: (embedded, %zu)", vectors.n);
            if (a.filter) fprintf(stderr, "  filter: %s", a.filter);
            fputc('\n', stderr);
        }
    }
    /* TLS certs: use --certs-dir if given, else try the standard search,
     * else auto-generate to a temp dir that is cleaned up at exit. */
    char certs_buf[4096];
    char auto_certs_dir[4096] = {0};
    const char *certs = resolve_certs_dir(&a, certs_buf);
    if (!certs) {
        if (auto_generate_certs(auto_certs_dir) != 0) {
            fputs("Error: could not generate TLS test certs. Ensure openssl is on PATH.\n", stderr);
            vector_list_free(&vectors);
            manifest_free(manifest);
            return 2;
        }
        certs = auto_certs_dir;
    }

    /* Output directory: all generated artifacts go under output/. */
    mkdir("output", 0755);
    mkdir("output/bodies", 0755);
    /* Set LACE_BODIES_DIR so executors write body files here, not in cwd.
     * Set LACE_RESULT_PATH=false to suppress result-file saving (the testkit
     * captures stdout — disk copies are unnecessary noise).
     * Use overwrite=0 so explicit env vars take precedence. */
    setenv("LACE_BODIES_DIR", "output/bodies", 0);
    setenv("LACE_RESULT_PATH", "false", 0);

    /* Resolve omit set: CLI --omit wins; else manifest's [conformance].omit.
     * Owned tokens go in omit_tokens; `cfg.omit` just points at them. */
    char  *omit_buf = NULL;
    const char **omit_tokens = NULL;
    size_t omit_count = 0;
    if (a.omit_csv) {
        omit_buf = strdup(a.omit_csv);
        if (omit_buf) {
            /* count commas + 1 for token capacity */
            size_t cap = 1;
            for (const char *p = omit_buf; *p; p++) if (*p == ',') cap++;
            omit_tokens = calloc(cap + 1, sizeof(char *));
            if (omit_tokens) {
                char *save = NULL;
                for (char *tok = strtok_r(omit_buf, ",", &save); tok;
                     tok = strtok_r(NULL, ",", &save)) {
                    while (*tok == ' ' || *tok == '\t') tok++;
                    char *end = tok + strlen(tok);
                    while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
                    if (*tok) omit_tokens[omit_count++] = tok;
                }
            }
        }
    } else if (manifest && manifest->omit) {
        omit_tokens = (const char **)manifest->omit;
        omit_count  = manifest->omit_count;
    }

    /* Extension-owned vectors: symlink extensions/ into the vectors tree
     * so the recursive walk discovers them. Skip if --omit extensions. */
    char ext_link_path[4096] = {0};
    if (!omit_contains(omit_tokens, omit_count, "extensions") && vdir) {
        char edir_buf[4096];
        const char *edir = resolve_extensions_dir(&a, vdir, edir_buf);
        if (!edir) {
            fprintf(stderr,
                "Error: extensions/ directory not found and extensions are\n"
                "not omitted. Pass --omit extensions, --extensions-dir <path>,\n"
                "or set $LACE_CONFORMANCE_EXTENSIONS.\n");
            vector_list_free(&vectors);
            manifest_free(manifest);
            if (a.omit_csv) { free(omit_buf); free(omit_tokens); }
            return 2;
        }
        if (create_extensions_symlink(vdir, edir, ext_link_path) != 0) {
            vector_list_free(&vectors);
            manifest_free(manifest);
            if (a.omit_csv) { free(omit_buf); free(omit_tokens); }
            return 2;
        }
        /* Re-walk the vectors dir — the symlink makes extension vectors
         * appear under vectors/_extensions/. Rather than re-walking the
         * whole tree, just walk the symlink subtree and merge. */
        vector_list_t ext_vectors;
        if (vector_load_dir(ext_link_path, a.filter, &ext_vectors) == 0
            && ext_vectors.n > 0) {
            if (a.report == REPORT_TEXT) {
                fprintf(stderr, "── extension vectors: %s (%zu found)\n",
                        edir, ext_vectors.n);
            }
            vector_list_merge(&vectors, &ext_vectors);
        } else {
            vector_list_free(&ext_vectors);
        }
    }

    if (vectors.n == 0) {
        fprintf(stderr, "No vectors matched.\n");
        remove_extensions_symlink(ext_link_path);
        vector_list_free(&vectors);
        return 0;
    }

    if (a.report == REPORT_TAP) {
        printf("TAP version 13\n1..%zu\n", vectors.n);
    }

    runner_config_t cfg = {0};
    cfg.executor_cmd    = a.executor_cmd;
    cfg.manifest        = manifest;
    cfg.timeout_seconds = a.timeout_seconds;
    cfg.report          = a.report;
    cfg.output_path     = a.output_path;
    cfg.certs_dir       = certs;
    cfg.omit            = omit_tokens;
    cfg.omit_count      = omit_count;

    /* Allocate JUnit reporter when requested, regardless of on-screen format. */
    bool want_junit = (a.report == REPORT_JUNIT) || (a.output_path != NULL);
    if (want_junit) cfg.junit = junit_reporter_new("lace-conformance");

    runner_summary_t sum;
    int rc = runner_run_all(&vectors, &cfg, &sum);

    /* Spec §17.4: conformance label. */
    const char *conformance_label;
    if (sum.failed > 0) {
        conformance_label = "non-compliant";
    } else if (omit_count > 0) {
        conformance_label = "compliant-partial";
    } else {
        conformance_label = "compliant";
    }

    if (a.report == REPORT_TEXT) {
        fprintf(stderr, "\n── %zu vectors: %zu passed, %zu failed, %zu skipped ──\n",
                sum.total, sum.passed, sum.failed, sum.skipped);
        fprintf(stderr, "── conformance: %s", conformance_label);
        if (omit_count > 0) {
            fprintf(stderr, " (omit:");
            for (size_t i = 0; i < omit_count; i++) {
                fprintf(stderr, "%s%s", i == 0 ? " " : ",", omit_tokens[i]);
            }
            fprintf(stderr, ")");
        }
        fputc('\n', stderr);
    } else if (a.report == REPORT_TAP) {
        printf("# conformance: %s", conformance_label);
        if (omit_count > 0) {
            printf(" (omit:");
            for (size_t i = 0; i < omit_count; i++) {
                printf("%s%s", i == 0 ? " " : ",", omit_tokens[i]);
            }
            printf(")");
        }
        putchar('\n');
    }

    if (cfg.junit) {
        const char *out = a.output_path ? a.output_path : "output/lace-conformance.xml";
        if (junit_reporter_write(cfg.junit, out) != 0) {
            fprintf(stderr, "Error: could not write JUnit report to %s\n", out);
            rc = rc ? rc : 2;
        } else if (a.report == REPORT_TEXT) {
            fprintf(stderr, "── JUnit report: %s\n", out);
        }
        junit_reporter_free(cfg.junit);
    }

    remove_extensions_symlink(ext_link_path);
    if (auto_certs_dir[0]) rmtree_main(auto_certs_dir);
    vector_list_free(&vectors);
    manifest_free(manifest);
    if (a.omit_csv) {
        free(omit_buf);
        free(omit_tokens);
    }
    return rc;
}
