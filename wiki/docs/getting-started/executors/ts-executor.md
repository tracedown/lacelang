# TypeScript Executor

Reference TypeScript implementation of Lace, conformant to spec version
**0.9.1** (171/171 conformance vectors). Passes the same test suite as the
canonical Python executor and is fully interchangeable.

The implementation is split into two packages following the
[packaging rule](../../implementers/packaging.md):

| Package | Repository | Description |
|---|---|---|
| `@lacelang/validator` | [tracedown/lacelang-js-validator](https://github.com/tracedown/lacelang-js-validator) | Lexer, parser, semantic validator. Zero runtime dependencies. |
| `@lacelang/executor` | [tracedown/lacelang-js-executor](https://github.com/tracedown/lacelang-js-executor) | HTTP runtime, assertion evaluation, cookie jars, extension dispatch. Depends on the validator. |

Requires Node.js **18+**.

---

## Installation

```bash
npm install @lacelang/executor
```

This automatically installs `@lacelang/validator` as a dependency.

Or from source:

```bash
npm install git+https://github.com/tracedown/lacelang-js-executor.git
```

---

## CLI usage

The executor CLI exposes three subcommands matching the
[testkit contract](../../implementers/index.md):

### `parse` -- syntax check

```bash
lacelang-executor parse script.lace
```

Outputs the AST as JSON. Parsing is delegated to `@lacelang/validator`.

### `validate` -- semantic checks

```bash
lacelang-executor validate script.lace \
    --vars-list vars.json \
    --context context.json
```

Runs the parser and semantic validator. Reports structured errors and
warnings. No HTTP calls are made.

### `run` -- full execution

```bash
lacelang-executor run script.lace \
    --vars vars.json \
    --prev prev.json \
    --pretty
```

Parses, validates, executes, and emits a
[ProbeResult](../../result/index.md) JSON.

#### Run flags

| Flag | Description |
|---|---|
| `--vars <file>` | JSON object with script variable values (`$var`). |
| `--var KEY=VALUE` | Inject a single variable (repeatable, overrides `--vars`). VALUE is parsed as JSON when valid, otherwise kept as a string. |
| `--prev-results <file>` | Previous result JSON, making `prev` available in expressions. `--prev` is a short alias. |
| `--config <file>` | Explicit path to a `lace.config` TOML file. |
| `--env <name>` | Select `[lace.config.<name>]` section (overrides `LACE_ENV`). |
| `--enable-extension <name>` | Activate a built-in extension (repeatable). |
| `--save-to <path>` | Persist the result to disk. Directory: timestamped JSON. File: overwrite. `"false"`: skip. |
| `--bodies-dir <path>` | Directory for request/response body files. |
| `--pretty` | Pretty-print the result JSON. |

---

## Library API

### `LaceExecutor`

The central entry point. Holds resolved config and registered extensions.

```typescript
import { LaceExecutor } from "@lacelang/executor";

// Point to the lace/ directory -- config loaded once
const executor = new LaceExecutor("lace");

// Or override the config path directly
const executor = new LaceExecutor("lace", { env: "staging" });
```

#### Constructor options

| Parameter | Type | Default | Description |
|---|---|---|---|
| `root` | `string \| null` | `null` | Path to the `lace/` directory. Discovers `lace.config` inside it and resolves script names relative to `{root}/scripts/`. |
| `config` | `string \| null` | `null` | Explicit path to `lace.config` (overrides root-based discovery). |
| `env` | `string \| null` | `null` | Selects `[lace.config.{env}]` section (overrides `LACE_ENV`). |
| `extensions` | `string[] \| null` | `null` | Built-in extensions to activate (e.g. `["laceNotifications"]`). |
| `trackPrev` | `boolean` | `true` | Auto-store last result as `prev` for next run on each probe. |

### `LaceProbe`

A prepared, reusable script bound to its parent executor. Created by
`executor.probe()`.

```typescript
// Prepare a probe by name -- resolves to lace/scripts/health/health.lace
const probe = executor.probe("health");

// Run -- returns a ProbeResult dict
const result = await probe.run({ base_url: "https://api.example.com" });

// Run again -- prev result from last run injected automatically
const result2 = await probe.run();
```

#### Script resolution

The `script` argument is resolved in order:

1. Ends with `.lace` -- treated as a file path
2. `{root}/scripts/{script}/{script}.lace` exists -- name-based lookup
3. Exists as a file on disk -- read it
4. Otherwise -- treated as inline Lace source code

### Extensions

Built-in extensions are activated via config or constructor:

```typescript
const executor = new LaceExecutor("lace", {
  extensions: ["laceNotifications"],
});
```

Register third-party extensions:

```typescript
// Directory (finds myext.laceext + myext.config inside)
executor.extension("lace/extensions/myext");

// Explicit paths
executor.extension("path/to/custom.laceext", "path/to/custom.config");
```

### One-shot execution

```typescript
// No probe caching, no prev tracking
const result = await executor.run("lace/scripts/health/health.lace", { key: "val" });

// Inline source
const result = await executor.run(`
get("https://api.example.com/health")
    .expect(status: 200)
`);
```

---

## Low-level API

The stateless `runScript()` function is available for callers that need
full control over parsing, validation, and config:

```typescript
import { parse } from "@lacelang/validator";
import { runScript, loadConfig } from "@lacelang/executor";
import * as fs from "node:fs";

const ast = parse(fs.readFileSync("script.lace", "utf-8"));
const config = loadConfig({ explicitPath: "lace.config" });

const result = await runScript(ast, { key: "val" }, null, null, null, null, null, config);
```

---

## Configuration

There is exactly **one config file** per executor. The `env` parameter
selects a **section within that file**, not a different file.

```toml
# lace/lace.config

[executor]
maxRedirects = 10
maxTimeoutMs = 300000

# Staging overlay -- deep-merged on top of base
[lace.config.staging]
[lace.config.staging.executor]
maxTimeoutMs = 60000
```

### Resolution by constructor arguments

| Constructor | Config file | Env overlay |
|---|---|---|
| `new LaceExecutor("lace")` | `lace/lace.config` | none (base only) |
| `new LaceExecutor("lace", { env: "staging" })` | `lace/lace.config` | `[lace.config.staging]` merged on base |
| `new LaceExecutor(null, { config: "/path/lace.config", env: "prod" })` | `/path/lace.config` | `[lace.config.prod]` merged on base |
| `LACE_ENV=staging` + `new LaceExecutor("lace")` | `lace/lace.config` | `[lace.config.staging]` (from env var) |

---

## Return value

Both `probe.run()` and `executor.run()` return a `Promise<Record<string, unknown>>` matching
the [ProbeResult wire format](../../result/index.md):

```typescript
{
  outcome: "success",        // "success" | "failure" | "timeout"
  startedAt: "2026-04-21T10:00:00.000Z",
  endedAt:   "2026-04-21T10:00:01.234Z",
  elapsedMs: 1234,
  runVars:   {},             // run-scoped variables from .store()
  calls:     [...],          // per-call result records
  actions:   {},             // write-back variables, notifications, etc.
}
```

---

## User-Agent

Per spec [section 3.6](../../language/http-calls.md), this executor sets a
default `User-Agent` on outgoing requests:

```
User-Agent: lace-probe/<version> (lacelang-js)
```

Precedence (highest first): per-request `headers: { "User-Agent": ... }` --
`lace.config [executor].user_agent` -- the default above.
