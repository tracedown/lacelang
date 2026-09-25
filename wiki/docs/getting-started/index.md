# Getting Started

## Prerequisites

You need a Lace executor -- a program that parses `.lace` files, runs the HTTP calls, evaluates assertions, and returns a structured JSON result.

!!! info "Executor implementations"
    The Lace specification is implementation-agnostic. Currently known executors:

    - **Python** (canonical reference) -- [`lacelang-executor`](https://github.com/tracedown/lacelang-python-executor) + [`lacelang-validator`](https://github.com/tracedown/lacelang-python-validator)
    - **TypeScript** (conformant) -- [`@lacelang/executor`](https://github.com/tracedown/lacelang-js-executor) + [`@lacelang/validator`](https://github.com/tracedown/lacelang-js-validator)
    - **Kotlin** (conformant) -- [`dev.lacelang:lacelang-kotlin-executor`](https://github.com/tracedown/lacelang-kotlin-executor) + [`dev.lacelang:kotlin-validator`](https://github.com/tracedown/lacelang-kotlin-validator)

    Any executor that passes the [conformance testkit](../implementers/index.md#4-pass-the-testkit) -- which verifies every item in the [core checklist](../implementers/checklist-core.md) -- is a valid Lace executor.

## Installing the Python reference executor

```bash
pip install lacelang-executor
```

This installs the `lacelang-executor` CLI and the `lacelang-validator` dependency (parser + semantic checks, no network dependencies), which provides the standalone `lacelang-validate` CLI. Python 3.11 or newer is required.

See the [Python Executor](executors/python-executor.md) or [TypeScript Executor](executors/ts-executor.md) page for full installation options and the programmatic API.

## Running your first script

**1. Create a file called `health.lace`:**

```lace
get("https://www.google.com/")
    .expect(status: 200)
    .check(totalDelayMs: { value: 3000 })
```

This probe does three things:

- Sends a GET request to `https://www.google.com/`
- Hard-fails if the status is not 200
- Soft-fails if the total response time reaches 3 seconds (the failure is recorded, the run continues)

**2. Run it:**

```bash
lacelang-executor run health.lace --pretty
```

**3. See the result** (abridged -- the key fields only):

```json
{
  "outcome": "success",
  "elapsedMs": 626,
  "calls": [
    {
      "index": 0,
      "outcome": "success",
      "response": {
        "status": 200,
        "responseTimeMs": 625,
        "dnsMs": 18,
        "connectMs": 38,
        "tlsMs": 48,
        "ttfbMs": 518,
        "transferMs": 87
      },
      "assertions": [
        { "method": "expect", "scope": "status", "outcome": "passed",
          "actual": 200, "expected": 200 },
        { "method": "check", "scope": "totalDelayMs", "outcome": "passed",
          "actual": 625, "expected": 3000 }
      ]
    }
  ]
}
```

The full result also includes request and response headers, DNS and TLS metadata, the resolved call config, redirects, and warnings. Response bodies are not saved by default -- each call records `bodyPath: null` with `bodyNotCapturedReason: "notRequested"`; pass `--save-body` or `--bodies-dir` to write them to disk. The `--pretty` flag formats the JSON for readability.

## CLI overview

The executor CLI has three subcommands:

### `parse` -- parse without validation

```bash
lacelang-executor parse script.lace
```

Outputs the AST as JSON. Useful for editor integrations and debugging syntax issues.

### `validate` -- parse and validate

```bash
lacelang-executor validate script.lace
```

Runs the parser and the semantic validator. Reports errors (which block execution) and warnings (which do not). No HTTP calls are made.

### `run` -- parse, validate, and execute

```bash
lacelang-executor run script.lace [options]
```

Key flags:

| Flag | Description |
|---|---|
| `--vars <file>` | JSON file with script variables (`$var` values). |
| `--var KEY=VALUE` | Inject a single variable. Repeatable; overrides the same key from `--vars`. |
| `--prev-results <file>` | Previous result JSON, making `prev` available in expressions. `--prev` is a short alias. |
| `--pretty` | Pretty-print the result JSON. |
| `--save-to <path>` | Override the result save path for this run (directory, file path, or `false`). |
| `--save-body` | Save response bodies for this run (to the result path). |
| `--bodies-dir <dir>` | Save response bodies to this directory (implies body saving). |
| `--enable-extension <name>` | Activate an extension for this run. Repeatable. |
| `--config <file>` | Path to a `lace.config` TOML file. |
| `--env <name>` | Select the `[lace.config.<name>]` environment section (overrides `LACE_ENV`). |

**Example with variables:**

```bash
lacelang-executor run script.lace \
  --vars vars.json \
  --var API_KEY=sk-test-123 \
  --prev-results last_result.json \
  --pretty
```

Where `vars.json` (passed explicitly -- it is never loaded automatically) might contain:

```json
{
  "BASE_URL": "https://api.example.com",
  "admin_email": "admin@example.com"
}
```

## Next steps

- **[Why Lace?](why-lace.md)** -- how Lace compares to existing tools and why a dedicated probe language matters.
- **[Examples](examples.md)** -- five annotated example scripts covering common monitoring patterns.
- **[Language](../language/index.md)** -- the full language reference: HTTP calls, assertions, variables, chaining.
