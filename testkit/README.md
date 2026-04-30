# lacelang-testkit

The canonical conformance test harness for Lace executors. Distributed as a
single self-contained native binary (`lace-conformance`) via system package
managers (apt, brew, opkg, etc.).

## What it tests

Every implementation of a Lace executor (Python, JavaScript, Kotlin, C, ...)
must pass the same battery of conformance tests to be considered
**Lace-Conformant**. The testkit invokes the executor as a subprocess through
a standardised CLI contract, captures structured output, and diffs against
expected results.

### Core vectors (mirroring `lace-spec.md` sections)

| Dir                          | Section                        | Type        | Count |
|------------------------------|--------------------------------|-------------|-------|
| `vectors/01_parsing/`        | Parsing                        | `parse`     | 27    |
| `vectors/02_validation/`     | Validation                     | `validate`  | 24    |
| `vectors/03_variables/`      | Variable handling              | `execute`   | 4     |
| `vectors/04_null_semantics/` | Null semantics                 | `execute`   | 10    |
| `vectors/05_http_execution/` | HTTP/HTTPS execution           | `execute`   | 26    |
| `vectors/06_cookie_jar/`     | Cookie jar                     | `execute`   | 7     |
| `vectors/07_chain_methods/`  | Chain method execution         | `execute`   | 10    |
| `vectors/08_body_matching/`  | Body matching                  | `execute`   | 8     |
| `vectors/09_prev_access/`    | prev access                    | `execute`   | 5     |
| `vectors/10_failure_cascade/`| Failure cascade                | `execute`   | 4     |
| `vectors/11_result_structure/`| Result structure              | `execute`   | 13    |
| `vectors/12_body_storage/`   | Body storage                   | `execute`   | 5     |
| `vectors/13_extension_core/` | Extension interface (core)     | `extension` | 7     |
| `vectors/14_config/`         | Configuration                  | `execute`   | 10    |
| **Core total**               |                                |             | **160** |

### Extension-owned vectors

Extensions ship their own vectors alongside their `.laceext` files. The
harness discovers them at runtime by symlinking the `extensions/` directory
into the vectors tree.

| Extension          | Count | Type        |
|--------------------|-------|-------------|
| laceNotifications  | 8     | `extension` |
| laceBaseline       | 7     | `extension` |
| configDemo (test)  | 2     | `extension` |
| hookTrace (test)   | 1     | `extension` |
| **Extension total**| **18**|             |

Each vector is a JSON file conforming to
`../specs/schemas/conformance-vector.json`.

## Usage

```bash
# Run the full suite against an executor:
lace-conformance -c "myexec"

# Or with an executor manifest:
lace-conformance -m /path/to/lace-executor.toml

# Filter by category or test id:
lace-conformance -c "myexec" --filter "07_chain_methods"
lace-conformance -c "myexec" --filter "expect_evaluates"

# Output formats:
lace-conformance -c "myexec" --report tap
lace-conformance -c "myexec" --report junit --output report.xml
```

## Executor CLI contract

Every executor must implement three subcommands. See
`../specs/schemas/executor-manifest.json` for the manifest format that
maps these to whatever CLI shape the executor uses.

```
<exec> parse <script.lace>
  stdout: { "ast": <AST> }                        on success
        | { "errors": [<ExpectedError>...] }      on parse failure
  exit:   0 / 1

<exec> validate <script.lace> [--vars-list <list.json>] [--context <ctx.json>]
  stdout: { "errors": [...], "warnings": [...] }
  exit:   0 if no errors, else 1

<exec> run <script.lace> --vars <vars.json> [--prev <prev.json>] [--config <config>]
  stdout: ProbeResult JSON
  exit:   0 success, 1 failure, 2 timeout, 3 internal error
```

## Output directory

All files generated during a conformance run are placed under `output/` in
the working directory:

```
output/
  lace-conformance.xml    JUnit XML report (when --report junit or --output)
  bodies/                 Response body storage (LACE_BODIES_DIR)
```

Temporary per-vector files (scripts, configs, vars) go to `/tmp/` and are
cleaned up after each test.

## TLS / HTTPS

HTTPS scenarios are **mandatory** — every Lace executor must handle TLS.
The harness auto-generates test certs on each run via `openssl` and cleans
them up at exit.

Requirements:
- **Build time**: OpenSSL dev headers (`libssl-dev` / `openssl` via brew)
- **Run time**: `openssl` CLI on PATH (for cert generation)

Override with `--certs-dir <path>` to use pre-generated certs (useful for
CI caching). Generate manually with `lace-conformance --generate-certs`.

TLS test scenarios:
- `valid` — CA-signed cert for 127.0.0.1 (executor must accept)
- `expired` — cert with elapsed validity (executor must reject)
- `wrong_host` — cert for wrong hostname (executor must reject)
- `self_signed` — self-signed cert, no CA chain (executor must reject)

## Architecture

The harness is a single C binary with a static OpenSSL link. This gives
one-line installation on every supported platform and eliminates runtime
data-file discovery.

For each `execute` or `extension` vector:

1. Bring up the bundled HTTP/HTTPS server with the vector's `http_mock`
   scenario loaded, listening on a dynamically-allocated port.
2. For HTTPS scenarios, load the cert matching the vector's `tls_scenario`.
3. Substitute `{port}` in the vector's `variables.BASE_URL`, write
   `vars.json` and (if present) `prev.json` to a temp dir.
4. Invoke the executor's `run` subcommand per the manifest.
5. Capture stdout, parse as `ProbeResult`.
6. Strip fields named in the vector's `expected.ignore` list (plus default
   ignores for timestamps and body paths).
7. Diff actual vs expected. Report.

For `parse` and `validate` vectors, no server is needed — just invoke
and diff.

## Building

Requires GCC or Clang, GNU Make, `pthread`, and OpenSSL dev libraries.
No other dependencies — the vector embedding tool is built from C as part
of the build when `EMBED_VECTORS=1`.

```bash
cd src
make                        # produces build/lace-conformance
make EMBED_VECTORS=1        # same, plus every vector embedded at build time
                            # (harness is now fully self-contained)
```

Distribution packages (deb, Homebrew, opkg) are scaffolded under
[`packaging/`](packaging/) — see its README for per-target instructions.

## Executor manifest (`-m`)

For executors whose invocation needs more than a bare `<cmd> parse
script.lace`, ship a `lace-executor.toml` in the executor's repo root.
The testkit reads it, substitutes placeholders in the per-subcommand
templates, and invokes the resulting argv:

```toml
[executor]
name        = "lacelang-executor"
version     = "0.9.1"
language    = "python"
conforms_to = ">=0.9.1"

[adapter]
parse    = "python3 -m lacelang_executor parse {script}"
validate = "python3 -m lacelang_executor validate {script} --vars-list {vars_list} --context {context}"
run      = "python3 -m lacelang_executor run {script} --vars {vars} --prev {prev}"
timeout_seconds = 30

[adapter.env]
LACE_BODIES_DIR = "/tmp/lace-bodies"
```

Placeholders: `{script}` `{vars}` `{vars_list}` `{context}` `{prev}`
`{config}`. All aux placeholders are always available — when a vector
doesn't declare the underlying input, the testkit writes the appropriate
empty JSON (`{}`, `[]`, `null`) to the temp path.

## Adding new vectors

1. Pick the right category directory under `vectors/`.
2. Copy an existing vector as a template.
3. Run `npm run check-vectors` from `../specs/tools/` to validate format
   and error-code references.
4. Run `lace-conformance` against a reference executor to confirm the
   new vector passes.
