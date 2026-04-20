# Configuration

Lace uses a TOML configuration file called `lace.config`. Every setting has a default, so the file is entirely optional.

## File Format

```toml
[executor]
extensions = []
maxRedirects = 10
maxTimeoutMs = 300000

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
| `maxRedirects` | `10` | System-wide redirect limit. Scripts cannot exceed this. |
| `maxTimeoutMs` | `300000` | System-wide timeout limit (5 minutes). Scripts cannot exceed this. |

### result

Controls where results are saved.

| Field | Default | Description |
|---|---|---|
| `path` | `"."` | Directory or file path for result JSON. See [Result Path](#result-path). |

### result.bodies

Controls where request/response body files are stored.

| Field | Default | Description |
|---|---|---|
| `dir` | Same as `result.path` | Directory for body files |

Body files follow the naming convention: `{dir}/call_{index}_{request|response}.{ext}`

### extensions

Each extension gets its own section under `[extensions.{name}]`:

```toml
[extensions.laceNotifications]
laceext = "builtin:laceNotifications"
prev_results = false

[extensions.laceLogging]
laceext = "builtin:laceLogging"
level = "warn"
include_in_result = true
stdout = false

[extensions.myCustomExtension]
laceext = "./extensions/myExtension.laceext"
api_key = "env:MY_EXT_API_KEY"
```

## Defaults Table

| Field | Default |
|---|---|
| `executor.extensions` | `[]` |
| `executor.maxRedirects` | `10` |
| `executor.maxTimeoutMs` | `300000` |
| `result.path` | `"."` |
| `result.bodies.dir` | Same as `result.path` |

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

1. **CLI flags** (`--vars`, `--prev-results`, `--save-to`, `--config`, `--env`)
2. **`lace.config`** in the script's directory
3. **`lace.config`** in the working directory
4. **Built-in defaults**

```bash
# CLI flags override everything
lace run script.lace --save-to ./output.json --vars production.json

# Specify a custom config file
lace run script.lace --config ./configs/staging.toml
```

!!! note "Config file is optional"
    If no `lace.config` file exists, all defaults apply. You only need a config file when you want to change limits, activate extensions, or customize result storage.
