# Executors

## What is an executor?

Lace is a language, not a program. A `.lace` file is plain text — it
cannot run itself. An **executor** is the program that reads a `.lace`
script, makes the HTTP calls it describes, evaluates the assertions,
and returns a structured JSON result (a `ProbeResult`).

```
  .lace script          executor              ProbeResult JSON
 ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐
 │ get(url)    │───>│ parse        │───>│ { outcome, calls,   │
 │ .expect(...)│    │ validate     │    │   assertions,       │
 │ .store(...) │    │ execute HTTP │    │   timing, actions } │
 └─────────────┘    │ evaluate     │    └─────────────────────┘
                    └──────────────┘
```

The executor is deliberately side-effect-free. It does not send alerts,
write dashboards, or decide retry policy. It returns data and exits.
The **backend** — the monitoring platform, CI pipeline, or cron job
that invoked the executor — decides what to do with the result.

## How executors work

Every executor implements three CLI subcommands:

| Subcommand | Purpose |
|---|---|
| `parse <script.lace>` | Parse the script and return the AST (or parse errors) |
| `validate <script.lace>` | Parse + validate and return errors/warnings |
| `run <script.lace>` | Parse + validate + execute and return a `ProbeResult` |

```bash
# Parse only — check syntax
lacelang-executor parse probe.lace

# Validate — check syntax + semantic rules
lacelang-executor validate probe.lace --vars-list vars.json

# Run — execute the probe
lacelang-executor run probe.lace --vars vars.json --pretty
```

The `--vars` flag injects variables (`$var` references in the script).
The `--prev-results` flag provides the previous run's result for
cross-run comparisons via `prev`.

## Conformance

A Lace executor is **conformant** when it passes the conformance test
suite shipped in the `testkit/` directory. The testkit is a C binary
(`lace-conformance`) that invokes the executor as a subprocess, feeds
it test scripts, and diffs the output against expected results:

```bash
lace-conformance -c "lacelang-executor"
```

The test suite covers 155+ core vectors across parsing, validation,
variable handling, null semantics, HTTP execution, cookie jars, chain
methods, body matching, previous results, failure cascade, result
structure, body storage, and configuration.

An executor that passes the full suite is **Lace-Conformant**. Partial
conformance is allowed — see [Conformance Levels](../../implementers/conformance-levels.md)
for how to declare omissions.

## Building your own executor

Executors can be built in any language. The spec, grammar, and schemas
define the contract:

- **[Grammar](../../reference/grammar.md)** — the EBNF that defines valid syntax
- **[Result Schema](../../reference/result-schema.md)** — the JSON structure every executor must produce
- **[Error Codes](../../reference/error-codes.md)** — the error codes every validator must emit
- **[Core Checklist](../../implementers/checklist-core.md)** — every behaviour an executor must implement
- **[Extension Checklist](../../implementers/checklist-extensions.md)** — extension system requirements

See [For Implementers](../../implementers/index.md) for a guided walkthrough.

To register your executor, open a PR adding it to the table below.
The maintainers will review your implementation and verify conformance.

## Known executor implementations

| Package | Language | Conformance | Spec Version | Repository |
|---|---|---|---|---|
| `lacelang-executor` | Python | **Canonical** | 0.9.0 | [tracedown/lacelang-python-executor](https://github.com/tracedown/lacelang-python-executor) |
| `@lacelang/executor` | TypeScript | **Conformant** | 0.9.0 | [tracedown/lacelang-js-executor](https://github.com/tracedown/lacelang-js-executor) |

Both implementations pass the full 171-vector conformance suite. The Python executor is the **canonical reference** — the spec is developed and verified against it. The TypeScript executor is fully interchangeable and targets the same spec version.

Each implementation splits into a **validator** (parser + semantic checks, zero network dependencies) and an **executor** (HTTP runtime). See the [packaging rules](../../implementers/packaging.md) for why.

| Validator | Language | Repository |
|---|---|---|
| `lacelang-validator` | Python | [tracedown/lacelang-python-validator](https://github.com/tracedown/lacelang-python-validator) |
| `@lacelang/validator` | TypeScript | [tracedown/lacelang-js-validator](https://github.com/tracedown/lacelang-js-validator) |
| `lacelang-kt-validator` | Kotlin | [tracedown/lacelang-kt-validator](https://github.com/tracedown/lacelang-kt-validator) |

See the [Python Executor](python-executor.md) and [TypeScript Executor](ts-executor.md) pages for installation, CLI usage, and the programmatic API.
