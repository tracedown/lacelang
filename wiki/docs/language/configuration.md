# Configuration

Lace uses a TOML configuration file called `lace.config`. Every setting has a default, so the file is entirely optional.

## File Format

```toml
[executor]
extensions = []
maxRedirects = 10
maxTimeoutMs = 300000
# user_agent = "acme-monitor/2026.09"

[result]
path = "./lace_results"

[result.bodies]
dir = "./lace_results/bodies"
```

## Config Sections

### executor

Controls runtime behaviour and limits.

| Field | Default | Description |
|---|---|---|
| `extensions` | `[]` | List of extensions to activate (must have `.laceext` files) |
| `maxRedirects` | `10` | System-wide redirect limit. A script `redirects.max` above it is a validation error. |
| `maxTimeoutMs` | `300000` | System-wide timeout limit (5 minutes). A script `timeout.ms` above it is a validation error. |
| `user_agent` | unset | Outgoing `User-Agent` for every request, used verbatim. Unset means the default `lace-probe/<executor-version> (<implementation-name>)`. A per-call `User-Agent` header still wins. See [Default Headers](http-calls.md#default-headers). |

### result

Controls where results are saved.

| Field | Default | Description |
|---|---|---|
| `path` | `"."` | Directory or file path for result JSON. See [Result Path](#result-path). |

### result.bodies

Controls whether and where response body files are stored.

| Field | Default | Description |
|---|---|---|
| `dir` | `false` | Directory for response body files. A path string saves bodies there; `false` does not save them. |

Body files follow the naming convention `{dir}/call_{index}_response.{ext}`, and the call record's `response.bodyPath` holds the absolute path. Request bodies are never saved (they are already in the script).

When bodies are not saved, `response.bodyPath` is `null` and `response.bodyNotCapturedReason` says why --- `"notRequested"` when body saving is off.

### extensions

Activating an extension and configuring it are separate. An extension is activated by listing it in `[executor].extensions` (or with `--enable-extension NAME` for a single run). Each extension can then be configured in its own `[extensions.{name}]` section:

```toml
[executor]
extensions = ["laceNotifications", "myCustomExtension"]

[extensions.laceNotifications]
laceext = "builtin:laceNotifications"
timeout_message = "Request timed out"

[extensions.myCustomExtension]
laceext = "./extensions/myExtension.laceext"
api_key = "env:MY_EXT_API_KEY"
```

`laceext` is the path to the `.laceext` file (default: the one bundled with the executor). The other keys are extension-specific and are read by the extension's rules as `config.<key>`. An `[extensions.{name}]` section for an extension that is not activated is silently ignored --- `laceext = "builtin:X"` on its own activates nothing.

Extensions may ship a companion `{name}.config` file next to their `.laceext` file with default values for their config fields. Those defaults are the base; any key also set in `lace.config` overrides them, and keys absent from `lace.config` keep the extension's defaults.

## Defaults Table

| Field | Default |
|---|---|
| `executor.extensions` | `[]` |
| `executor.maxRedirects` | `10` |
| `executor.maxTimeoutMs` | `300000` |
| `executor.user_agent` | unset |
| `result.path` | `"."` |
| `result.bodies.dir` | `false` |

## Result Path

The `result.path` field accepts three forms:

- **Directory path** (e.g. `"./lace_results"`) --- saves as `{dir}/{YYYY-MM-DD_HH-MM-SS}.json`, sortable with no collisions
- **Full file path** (e.g. `"./result.json"`) --- always overwrites that file
- **`false`** --- do not save the result to disk

Override for a single run with the `--save-to` CLI flag.

## Environment Variable Resolution

Any string value in the config can reference an environment variable:

```toml
[extensions.myExtension]
api_key = "env:MY_API_KEY"
api_key_with_fallback = "env:MY_API_KEY:default_value"
```

| Syntax | Behaviour |
|---|---|
| `"env:VARNAME"` | Resolves to the value of `VARNAME`. Error at startup if unset. |
| `"env:VARNAME:default"` | Resolves to `VARNAME` if set, otherwise uses `default`. |

## Environment Selection

Use environment-specific config sections for different deployment targets:

```toml
[lace.config.production]
executor.maxTimeoutMs = 10000

[lace.config.staging]
executor.maxTimeoutMs = 30000
```

Select the active environment with:

- The `LACE_ENV` environment variable
- The `--env` CLI flag (takes precedence over `LACE_ENV`)

## Config Resolution Order

Settings are resolved with this precedence (highest first):

1. **CLI flags** (`--vars`, `--var`, `--prev-results`, `--save-to`, `--save-body`, `--bodies-dir`, `--enable-extension`, `--env`, `--config`)
2. **`lace.config`** in the script's directory
3. **`lace.config`** in the working directory
4. **Built-in defaults**

The flags that override config values for a single run:

| Flag | Effect |
|---|---|
| `--save-to <path>` | Overrides `result.path`. |
| `--save-body` | Turns body saving on, setting `result.bodies.dir` to the result path (or the system temp directory). |
| `--bodies-dir <dir>` | Sets `result.bodies.dir` to `<dir>` (implies body saving). |
| `--enable-extension <name>` | Activates an extension as if listed in `executor.extensions`. Repeatable. |
| `--env <name>` | Selects the `[lace.config.<name>]` section. |
| `--config <file>` | Loads this config file instead of discovering one. |

The previous result (`--prev-results`) has no config-file equivalent; it is always supplied per run.

```bash
# CLI flags override everything
lacelang-executor run script.lace --save-to ./output.json --vars production.json

# Save response bodies for this run
lacelang-executor run script.lace --bodies-dir ./bodies

# Specify a custom config file
lacelang-executor run script.lace --config ./configs/staging.toml
```

!!! note "Config file is optional"
    If no `lace.config` file exists, all defaults apply. You only need a config file when you want to change limits, activate extensions, or customize result storage.
