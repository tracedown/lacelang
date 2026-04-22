# lacelang

## Definition

Lace, or lacelang, is a purpose-built HTTP probing declarative scripting microlanguage, 
specifically designed to be used in API monitoring tools.

### Brief list of features:
* Hard- and Soft- fail assertions
* Full request state monitoring
* Request chaining
* Runtime variables
* Scriptable extensions system
* Validation against previous result
* Write-back variables backend contract

This repository owns the **definition** of Lace. Canonical and community executor implementations
live in separate repositories and conform to this spec by passing the test suite shipped in
`testkit/`.


### Table of contents

- [Known executor implementations](#known-executor-implementations)
- [Examples](#examples)
- [Repository layout](#repository-layout)
- [Core ideas](#core-ideas)
- [Extensions](#extensions)
- [Verifying the spec artifacts](#verifying-the-spec-artifacts)
- [Contributing](#contributing)
- [Versioning](#versioning)

### Known executor implementations

| Package | Language | Conformance | Spec Version | Repository |
|---|---|---|---|---|
| `lacelang-executor` | Python | Canonical | 0.9.0 | [tracedown/lacelang-python-executor](https://github.com/tracedown/lacelang-python-executor) |
| `lacelang-validator` | Python | Canonical | 0.9.0 | [tracedown/lacelang-python-validator](https://github.com/tracedown/lacelang-python-validator) |
| `@lacelang/executor` | TypeScript | Conformant | 0.9.0 | [tracedown/lacelang-js-executor](https://github.com/tracedown/lacelang-js-executor) |
| `@lacelang/validator` | TypeScript | Conformant | 0.9.0 | [tracedown/lacelang-js-validator](https://github.com/tracedown/lacelang-js-validator) |
| `lacelang-kt-validator` | Kotlin | Conformant | 0.9.0 | [tracedown/lacelang-kt-validator](https://github.com/tracedown/lacelang-kt-validator) |

The Python implementation is the **canonical reference** — the spec is developed and verified against it. The TypeScript and Kotlin implementations pass the same conformance vectors and are fully interchangeable.

## Examples

> Results below are compacted for readability — full executor output
> (with complete headers, TLS metadata, DNS resolution, and timing
> breakdowns) lives in [`examples/`](examples/). Each subdirectory
> contains the `.lace` script and the anonymized `result.json` produced
> by the Python reference executor.

### Service status monitoring

```lace
get("https://www.google.com/")
    .expect(status: [200, 301, 302])
```

```json
{
  "outcome": "success",
  "elapsedMs": 626,
  "calls": [
    {
      "index": 0, "outcome": "success",
      "request": { "url": "https://www.google.com/", "method": "get" },
      "response": {
        "status": 200, "statusText": "OK",
        "responseTimeMs": 625, "dnsMs": 18, "connectMs": 38,
        "tlsMs": 48, "ttfbMs": 518, "transferMs": 87, "sizeBytes": 79192
      },
      "assertions": [
        { "method": "expect", "scope": "status", "op": "eq",
          "outcome": "passed", "actual": 200, "expected": [200, 301, 302] }
      ]
    }
  ]
}
```

### Time-scoped monitoring

Hard-fail if total response exceeds 3 seconds, soft-warn if individual
timing phases are slow:

```lace
get("https://www.google.com/", {
  headers: { Accept: "text/html" }
})
.expect(
  status: 200,
  totalDelayMs: { value: 3000 }
)
.check(
  dns:      { value: 80 },
  connect:  { value: 150 },
  tls:      { value: 200 },
  ttfb:     { value: 500 },
  transfer: { value: 400 }
)
```

```json
{
  "outcome": "success",
  "elapsedMs": 582,
  "calls": [
    {
      "index": 0, "outcome": "success",
      "response": {
        "status": 200, "statusText": "OK",
        "responseTimeMs": 580, "dnsMs": 5, "connectMs": 34,
        "tlsMs": 41, "ttfbMs": 488, "transferMs": 72
      },
      "assertions": [
        { "method": "expect", "scope": "status", "op": "eq",
          "outcome": "passed", "actual": 200, "expected": 200 },
        { "method": "expect", "scope": "totalDelayMs", "op": "lt",
          "outcome": "passed", "actual": 580, "expected": 3000 },
        { "method": "check", "scope": "dns", "op": "lt",
          "outcome": "passed", "actual": 5, "expected": 80 },
        { "method": "check", "scope": "connect", "op": "lt",
          "outcome": "passed", "actual": 34, "expected": 150 },
        { "method": "check", "scope": "tls", "op": "lt",
          "outcome": "passed", "actual": 41, "expected": 200 },
        { "method": "check", "scope": "ttfb", "op": "lt",
          "outcome": "passed", "actual": 488, "expected": 500 },
        { "method": "check", "scope": "transfer", "op": "lt",
          "outcome": "passed", "actual": 72, "expected": 400 }
      ]
    }
  ]
}
```

### Chained requests

POST to one endpoint, capture a value with `.store()`, then use it in a
subsequent request. If the first call fails, the second is skipped
(hard-fail cascade):

```lace
post("https://httpbin.org/post", {
  body: json({ email: "user@example.com", action: "login" })
})
.expect(status: 200)
.store({ "$$origin": this.body.origin })

get("https://httpbin.org/get", {
  headers: { Origin: "$$origin" }
})
.expect(status: 200)
.check(totalDelayMs: { value: 1000 })
```

```json
{
  "outcome": "success",
  "elapsedMs": 1614,
  "runVars": { "origin": "203.0.113.1" },
  "calls": [
    {
      "index": 0, "outcome": "success",
      "request": { "url": "https://httpbin.org/post", "method": "post" },
      "response": { "status": 200, "statusText": "OK", "responseTimeMs": 1039 },
      "assertions": [
        { "method": "expect", "scope": "status", "op": "eq",
          "outcome": "passed", "actual": 200, "expected": 200 }
      ]
    },
    {
      "index": 1, "outcome": "success",
      "request": { "url": "https://httpbin.org/get", "method": "get" },
      "response": { "status": 200, "statusText": "OK", "responseTimeMs": 558 },
      "assertions": [
        { "method": "expect", "scope": "status", "op": "eq",
          "outcome": "passed", "actual": 200, "expected": 200 },
        { "method": "check", "scope": "totalDelayMs", "op": "lt",
          "outcome": "passed", "actual": 558, "expected": 1000 }
      ]
    }
  ]
}
```

### Expect vs check — hard and soft failures

`.expect()` is a hard fail — if it fails, subsequent calls are skipped.
`.check()` is a soft fail — the failure is recorded but execution
continues:

```lace
get("https://www.google.com/")
.expect(status: 200)
.check(
  totalDelayMs: { value: 1000 },
  ttfb: { value: 200 }
)
```

When `ttfb` exceeds 200ms but everything else passes, the overall call
outcome is still `"success"` but the check is recorded as `"failed"`:

```json
{
  "outcome": "success",
  "elapsedMs": 637,
  "calls": [
    {
      "index": 0, "outcome": "success",
      "assertions": [
        { "method": "expect", "scope": "status", "op": "eq",
          "outcome": "passed", "actual": 200, "expected": 200 },
        { "method": "check", "scope": "totalDelayMs", "op": "lt",
          "outcome": "passed", "actual": 635, "expected": 1000 },
        { "method": "check", "scope": "ttfb", "op": "lt",
          "outcome": "failed", "actual": 505, "expected": 200 }
      ]
    }
  ]
}
```

### Custom notification templates per status code

Using `laceNotifications`, send different notification templates based
on which status code was returned. The `op_map` expression builder
resolves to the matching template before the notification is emitted:

```lace
get("$BASE_URL/api/orders")
.expect(
  status: {
    value: 200,
    options: {
      notification: op_map({
        "404": template("not_found_alert"),
        "500": template("server_error_alert"),
        "502": template("gateway_error_alert"),
        "503": template("service_unavailable_alert"),
        "default": template("unexpected_status_alert")
      })
    }
  }
)
```

When the endpoint returns 404, the assertion's `options` preserves the
original `op_map` config, but the emitted notification in `actions`
contains only the resolved `template`:

```json
{
  "outcome": "failure",
  "elapsedMs": 905,
  "calls": [
    {
      "index": 0, "outcome": "failure",
      "response": { "status": 404, "statusText": "NOT FOUND" },
      "assertions": [
        { "method": "expect", "scope": "status", "op": "eq",
          "outcome": "failed", "actual": 404, "expected": 200,
          "options": { "notification": { "tag": "op_map", "ops": { "...": "..." } } } }
      ]
    }
  ],
  "actions": {
    "notifications": [
      {
        "callIndex": 0, "conditionIndex": -1,
        "trigger": "expect", "scope": "status",
        "notification": { "tag": "template", "name": "not_found_alert" }
      }
    ]
  }
}
```

---

## Repository layout

```
lacelang/
├── specs/
│   ├── lace-spec.md           Prose: language semantics and behaviour
│   ├── lace-extensions.md     Prose: extension system
│   ├── checklist-core.md      Core executor compatibility checklist
│   ├── checklist-extensions.md Extension system compatibility checklist
│   ├── lacelang.g4            ANTLR4 grammar (canonical syntax)
│   ├── error-codes.json       Canonical validator error code registry
│   ├── notes/
│   │   ├── grammar.md         ANTLR4 grammar divergences from spec EBNF
│   │   └── extensions.md      Extension format implementation conventions
│   ├── schemas/
│   │   ├── ast.json           Internal AST (parser output, testkit comparison target)
│   │   ├── result.json        ProbeResult (executor wire output)
│   │   ├── laceext.json       .laceext file structure
│   │   ├── conformance-vector.json   Test vector format
│   │   ├── lace-config.json   Executor configuration file
│   │   ├── lace-ext-config.json   Extension config defaults file
│   │   └── executor-manifest.json    lace-executor.toml format
│   ├── grammar-tests/
│   │   ├── positive/          Sample .lace files that must parse
│   │   └── negative/          Sample .lace files that must produce parse errors
│   ├── tools/
│   │   ├── check-schemas.js   Compile every JSON schema, verify error-codes registry
│   │   ├── check-laceext.js   Validate every .laceext under ../extensions/
│   │   └── check-vectors.js   Validate every conformance vector under ../testkit/vectors/
│   └── Makefile               make verify — runs grammar tests, schema, extension, and vector checks
│
├── extensions/
│   ├── default/               Default extensions, recommended to be bundled with every executor
│   │   └── laceNotifications/
│   │       ├── laceNotifications.laceext
│   │       ├── laceNotifications.config
│   │       ├── README.md      Notification types, backend contract
│   │       └── vectors/       Extension-specific conformance vectors
│   │   └── laceBaseline/
│   │       ├── laceBaseline.laceext
│   │       ├── laceBaseline.config
│   │       ├── README.md      Configuration, backend contract
│   │       └── vectors/       Extension-specific conformance vectors
│   └── test/                  Test-only extensions for the conformance suite
│       ├── hookTrace/
│       │   ├── hookTrace.laceext
│       │   └── vectors/
│       ├── notifRelay/
│       │   └── notifRelay.laceext
│       ├── notifCounter/
│       │   └── notifCounter.laceext
│       ├── notifWatch/
│       │   └── notifWatch.laceext
│       ├── badNamespace/
│       │   └── badNamespace.laceext
│       └── configDemo/
│           ├── configDemo.laceext
│           └── configDemo.config
│
├── examples/                  Runnable scripts with real executor output
│   ├── service-status/
│   ├── time-scoped-monitoring/
│   ├── chained-requests/
│   ├── expect-vs-check/
│   └── notification-templates/
│
├── testkit/
│   ├── README.md              Architecture and usage
│   ├── src/                   C harness
│   ├── vectors/               System-level conformance vectors (checklist-core.md)
│   ├── certs/                 TLS cert generation script (certs generated at runtime)
│   ├── tools/                 Cert generation, packaging helpers
│   └── packaging/             Distribution metadata
│
├── wiki/                      Documentation site (mkdocs-material)
│   ├── mkdocs.yml             Site config and navigation
│   ├── Dockerfile             Build + serve static site
│   ├── railway.toml           Railway deployment config
│   └── docs/                  47 wiki pages
│       ├── getting-started/   Installation, examples, why Lace
│       ├── language/          Script authoring guide
│       ├── result/            ProbeResult format for backends
│       ├── extensions/        Extension authoring guide
│       ├── reference/         Grammar, error codes, schemas, scopes
│       ├── implementers/      Checklists, packaging, conformance
│       └── project/           Contributing, changelog, license
│
├── VERSION                   Canonical version string (all specs, schemas, testkit reference this)
├── CONTRIBUTING.md           How to contribute, register executors, propose spec changes
├── GOVERNANCE.md             Project governance and future foundation plans
└── CHANGELOG.md              Version history
```

---

## Core ideas

**The grammar is canonical.** `specs/lacelang.g4` defines the syntax. Every
parser implementation must accept exactly what the grammar accepts and
reject everything else. Where the grammar is more permissive than the spec
EBNF (notably for extension function calls and option blocks), the
validator enforces the strict rule — see `notes/grammar.md`.

**The schemas are canonical wire formats.** `specs/schemas/` defines the
JSON shapes for the internal AST, the executor result, extension files,
test vectors, configuration, and the executor manifest. Implementations
that produce or consume these structures must match the schemas exactly.

**The error-code registry is canonical.** `specs/error-codes.json` lists
every validator error and warning code. All validator implementations
must emit these codes — this is what makes cross-language conformance
possible. The conformance vector schema constrains expected error codes
to this enum.

**The testkit is the conformance arbiter.** A Lace executor is
"Lace-Conformant" iff `lace-conformance -c "<exec>"` exits 0. The kit
ships a bundled HTTP/HTTPS test server and a battery of test vectors
covering every behavioural item in `checklist-core.md`.

**Extensions are declarative TOML files.** The `.laceext` schema constrains
structure; rule and function bodies are an embedded language defined in
`lace-extensions.md §5` and parsed by the executor's extension processor
(not by the schema). See `extensions/` below.

---

## Extensions

The `extensions/` directory contains the canonical source for all
extensions defined in this repository. Each extension lives in its own
subdirectory with at minimum a `.laceext` file, and optionally a
`.config` defaults file (§2.3), a `README.md`, and a `vectors/`
directory containing extension-specific conformance test vectors.

The conformance runner symlinks `extensions/` into the vectors tree at
run time so extension vectors are discovered by the same recursive walk.
Pass `--omit extensions` to skip extension vectors entirely.

### `extensions/default/`

Default extensions that every conformant executor is recommended to bundle in the base distribution. Currently:

- **`laceNotifications/`** — Notification dispatch for assertion failures
  and call timeouts. See its `README.md` for the notification type
  system (`text`, `template`, `structured`, `op_map`) and the backend
  integration contract.
- **`laceBaseline/`** — Response timing baseline monitoring and spike
  detection. Tracks rolling averages of timing metrics across runs via
  `prev.runVars` and emits spike events when a metric exceeds the
  baseline by a configurable multiplier. Depends on `laceNotifications`
  for alert dispatch.

Executor implementations are recommended to ship copies of these files and register
them as `builtin:<name>` (e.g. `builtin:laceNotifications`). Executors should
support both flat (`extensions/name.laceext`) and subdirectory
(`extensions/name/name.laceext`) layouts.

### `extensions/test/`

Extensions that exist solely for the conformance test suite. They exercise
specific extension system features and are **not** intended for production
use:

| Extension | Purpose |
|---|---|
| `hookTrace/` | Emits a trace entry on every hook point (all 12). Verifies hook firing order and count. |
| `notifRelay/` | Requires `laceNotifications`; calls its exposed `pushNotification` function. Tests inter-extension function dispatch. |
| `notifCounter/` | Publishes a `runVars` entry on each call. Producer half of the require-read demo. |
| `notifWatch/` | Requires `notifCounter`; reads its published var. Consumer half — tests `require[]` base access. |
| `badNamespace/` | Deliberately emits an unprefixed `runVars` key. Tests that executors reject namespace violations. |
| `configDemo/` | Echoes extension config values to `result.actions`. Tests `.config` defaults and `lace.config` override merge. |

Executor test harnesses load these on demand (via `--enable-extension`)
when running the conformance vectors that reference them.

---

## Verifying the spec artifacts

```bash
cd specs
make verify
```

Runs:

1. **Grammar** — generates the parser from `lacelang.g4`, then parses
   every sample under `grammar-tests/{positive,negative}/`. Positives
   must parse cleanly; negatives must produce parse errors.
2. **Schemas** — compiles every JSON schema under `schemas/` and
   verifies the error-code registry has no duplicates.
3. **Extensions** — TOML-parses every `.laceext` under `../extensions/`
   and validates against the laceext schema.
4. **Vectors** — validates every conformance vector under
   `../testkit/vectors/` against the vector schema and cross-checks
   referenced error codes against the registry.

Requires Java (JDK 11+) for the grammar tests and Node.js for the
schema/vector checks. Both are downloaded/installed automatically into
`build/` and `tools/node_modules/` respectively.

---

## Contributing

### To the spec prose (`specs/*.md`)

Spec changes that affect executor behaviour or wire formats need to flow
through to the `.g4`, JSON schemas, and error-code registry. Run
`make verify` before opening a PR.

### To the grammar (`specs/lacelang.g4`)

If you change the grammar, add at least one sample to
`grammar-tests/positive/` or `grammar-tests/negative/` demonstrating
the change. If the change affects the AST structure, update
`schemas/ast.json` and add a `parse` conformance vector.

### To the schemas (`specs/schemas/*.json`)

Schema changes are breaking changes for every executor. Bump the schema
`$id` version and document the migration in a changelog entry.

### To the test vectors (`testkit/vectors/*.json`)

New vectors: pick the right `vectors/NN_*` directory, copy an existing
vector as a template, and run `npm run check-vectors` from `specs/tools/`
to validate the format.

### To the testkit C harness (`testkit/src/`)

See `testkit/README.md` for architecture and current status.

---

## Versioning

Spec, grammar, schemas, error codes, vectors, and the testkit all share
a single version line — currently **0.9.0** (pre-release). Any change to
the spec that requires executors to update is a version bump. Pre-1.0
versions may break compatibility freely; from 1.0.0 onward, breaking
changes follow semver (major bump).

Each executor declares which spec version it conforms to via its
`lace-executor.toml` `[executor].conforms_to` field.

See `CHANGELOG.md` for version history.

## Generative AI notice
Generative AI has been used to compose some of the specification documents
in this repository. This was done with a goal to ease the burden of
documentation, and to produce more readable specifications. No paid human
labor was replaced by that operation.
