# Python Executor

The canonical reference implementation of Lace, conformant to spec version
**0.9.1**. The Lace specification is developed and verified against this
implementation — conformance vectors, error codes, and wire-format schemas
are tested here before each spec release.

The implementation is split into two packages following the
[packaging rule](../../implementers/packaging.md):

| Package | Repository | Description |
|---|---|---|
| `lacelang-validator` | [tracedown/lacelang-python-validator](https://github.com/tracedown/lacelang-python-validator) | Lexer, parser, semantic validator. Zero network dependencies. |
| `lacelang-executor` | [tracedown/lacelang-python-executor](https://github.com/tracedown/lacelang-python-executor) | HTTP runtime, assertion evaluation, cookie jars, extension dispatch. Depends on the validator. |

Requires Python **3.10+**.

---

## Installation

```bash
pip install lacelang-executor
```

This automatically installs `lacelang-validator` as a dependency.

Or from source:

```bash
pip install git+https://github.com/tracedown/lacelang-python-executor.git
```

---

## CLI usage

The executor CLI exposes three subcommands matching the
[testkit contract](../../implementers/index.md):

### `parse` -- syntax check

```bash
lacelang-executor parse script.lace
```

Outputs the AST as JSON. Parsing is delegated to `lacelang-validator`.

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

#### Examples

```bash
# Basic health check
lacelang-executor run health.lace --pretty

# With variables and previous result
lacelang-executor run probe.lace \
    --vars vars.json \
    --var API_KEY=sk-test-123 \
    --prev-results last_result.json \
    --pretty

# Enable notifications extension
lacelang-executor run probe.lace --enable-extension laceNotifications
```

---

## Library API

The executor provides a programmatic API for embedding Lace in Python
applications. This is the recommended way to run Lace scripts from code
-- it parses the AST once, loads config once, and tracks previous results
automatically.

### Project layout

The executor expects Lace files under a dedicated `lace/` directory:

```
my-project/
  lace/
    lace.config                      # executor config (auto-discovered)
    extensions/                      # third-party extensions
      myext/
        myext.laceext
        myext.config
    scripts/
      health/
        health.lace                  # script (name = directory name)
        vars.json                    # default variables
        vars.staging.json            # env-specific variables
      auth-flow/
        auth-flow.lace
        vars.json
```

All paths are overridable at runtime -- the layout is a convention,
not a requirement.

### `LaceExecutor`

The central entry point. Holds resolved config and registered extensions.

```python
from lacelang_executor import LaceExecutor

# Point to the lace/ directory -- config loaded once
executor = LaceExecutor("lace")

# Or override the config path directly
executor = LaceExecutor(config="path/to/lace.config", env="staging")
```

#### Constructor parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `root` | `str \| None` | `None` | Path to the `lace/` directory. Discovers `lace.config` inside it and resolves script names relative to `{root}/scripts/`. |
| `config` | `str \| None` | `None` | Explicit path to `lace.config` (overrides root-based discovery). |
| `env` | `str \| None` | `None` | Selects `[lace.config.{env}]` section (overrides `LACE_ENV`). |
| `extensions` | `list[str] \| None` | `None` | Built-in extensions to activate (e.g. `["laceNotifications"]`). |
| `track_prev` | `bool` | `True` | Auto-store last result as `prev` for next run on each probe. |

### Registering extensions

```python
# Directory layout (finds myext.laceext + myext.config automatically)
executor.extension("lace/extensions/myext")

# Explicit paths
executor.extension("path/to/custom.laceext", "path/to/custom.config")
```

The `extension()` method returns a `LaceExtension` object with `name`,
`path`, and `config_path` attributes.

### `LaceProbe`

A prepared, reusable script bound to its parent executor. Created by
`executor.probe()`.

```python
# Prepare a probe by name -- resolves to lace/scripts/health/health.lace
# AST is parsed and validated once, reused across runs
probe = executor.probe("health")

# Run -- returns a ProbeResult dict
result = probe.run(vars={"base_url": "https://api.example.com"})

# Run again -- prev result from last run injected automatically
result = probe.run()

# All inputs accept file paths or dicts
result = probe.run(
    vars="lace/scripts/health/vars.staging.json",
    prev="results/last_run.json",    # explicit prev overrides auto-tracking
)
```

#### `probe()` parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `script` | `str` | -- | Script name (`"health"`), file path (`"path/to.lace"`), or inline source. |
| `always_reparse` | `bool` | `False` | Re-read script file on every `run()`. Useful during development. |

#### Script resolution

The `script` argument is resolved in order:

1. Ends with `.lace` -- treated as a file path
2. `{root}/scripts/{script}/{script}.lace` exists -- name-based lookup
3. Exists as a file on disk -- read it
4. Otherwise -- treated as inline Lace source code

#### `run()` parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `vars` | `str \| dict \| None` | `None` | Script variables -- dict or path to JSON. |
| `prev` | `str \| dict \| None` | `None` | Previous result -- dict or path to JSON. Overrides auto-tracking. |
| `reparse` | `bool` | `False` | Re-read script from disk for this run only. |

### One-shot execution

For scripts that don't need probe caching or prev tracking:

```python
# File path
result = executor.run("lace/scripts/health/health.lace", vars={"key": "val"})

# Inline source
result = executor.run('''
get("https://api.example.com/health")
    .expect(status: 200)
''')
```

### Development mode

Re-read and re-parse the script file on every `run()`:

```python
probe = executor.probe("health", always_reparse=True)
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

# Staging overlay -- deep-merged on top of base.
# Only the keys you specify are overridden; the rest is inherited.
[lace.config.staging]
[lace.config.staging.executor]
maxTimeoutMs = 60000           # overridden
# maxRedirects is inherited (10)

[lace.config.production]
[lace.config.production.executor]
user_agent = "lace-probe/0.9.1 (acme-platform)"
```

### Resolution by constructor arguments

| Constructor | Config file | Env overlay |
|---|---|---|
| `LaceExecutor("lace")` | `lace/lace.config` | none (base only) |
| `LaceExecutor("lace", env="staging")` | `lace/lace.config` | `[lace.config.staging]` merged on base |
| `LaceExecutor(config="/path/lace.config", env="prod")` | `/path/lace.config` | `[lace.config.prod]` merged on base |
| `LaceExecutor("lace", config="/other/lace.config")` | `/other/lace.config` | none (root still used for script names) |
| `LACE_ENV=staging` + `LaceExecutor("lace")` | `lace/lace.config` | `[lace.config.staging]` (from env var) |
| `LACE_ENV=staging` + `LaceExecutor("lace", env="prod")` | `lace/lace.config` | `[lace.config.prod]` (kwarg wins) |

The `config=` kwarg overrides where the file is loaded from.
The `env=` kwarg (or `LACE_ENV`) selects which section inside
that file is overlaid. They are independent.

---

## Return value

Both `probe.run()` and `executor.run()` return a `dict` matching the
[ProbeResult wire format](../../result/index.md):

```python
{
    "outcome": "success",        # "success" | "failure" | "timeout"
    "startedAt": "2026-04-20T10:00:00.000Z",
    "endedAt":   "2026-04-20T10:00:01.234Z",
    "elapsedMs": 1234,
    "runVars":   {},             # run-scoped variables from .store()
    "calls":     [...],          # per-call result records
    "actions":   {},             # write-back variables, notifications, etc.
}
```

---

## Low-level API

The stateless `run_script()` function is available for callers that need
full control over parsing, validation, and config:

```python
from lacelang_validator.parser import parse
from lacelang_executor.executor import run_script
from lacelang_executor.config import load_config

ast = parse(open("script.lace").read())
config = load_config(explicit_path="lace.config")

result = run_script(ast, script_vars={"key": "val"}, config=config)
```

---

## User-Agent

Per spec [section 3.6](../../language/http-calls.md), this executor sets a
default `User-Agent` on outgoing requests:

```
User-Agent: lace-probe/<version> (lacelang-python)
```

Precedence (highest first): per-request `headers: { "User-Agent": ... }` --
`lace.config [executor].user_agent` -- the default above.

---

## Architecture

```
lacelang-validator                    lacelang-executor
┌─────────────────┐                  ┌──────────────────────┐
│  lexer.py       │                  │  executor.py         │
│  parser.py      │──── AST dict ──>│  http_timing.py      │
│  validator.py   │                  │  config.py           │
│  errors.py      │                  │  laceext/            │
│  ast_fmt.py     │                  │    loader.py         │
│  cli.py         │                  │    dsl_parser.py     │
└─────────────────┘                  │    dsl_evaluator.py  │
                                     │  api.py (LaceExecutor)│
                                     │  cli.py              │
                                     └──────────────────────┘
```

The validator produces an AST dict (the wire format between packages).
The executor consumes the AST, makes HTTP requests, evaluates assertions,
runs extension rules, and emits a ProbeResult dict.
