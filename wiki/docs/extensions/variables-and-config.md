# Variables & Config

## Extension configuration

### Config file defaults

An extension ships a `.config` file alongside its `.laceext` file with default values:

```toml
# laceBaseline.config
[extension]
name    = "laceBaseline"
version = "1.0.0"

[config]
min_entries      = 5
spike_multiplier = 3.0
spike_action     = "include"
```

The `[extension]` header must match the `.laceext` file's `name` and `version`. If they differ, startup fails.

### lace.config overrides

Projects override config values in `lace.config` under `[extensions.<name>]`:

```toml
[extensions.laceBaseline]
laceext          = "builtin:laceBaseline"
min_entries      = 10
spike_multiplier = 2.5
spike_action     = "skip"
```

Environment variable substitution with `env:VARNAME` and `env:VARNAME:default` works in `lace.config` overrides:

```toml
[extensions.laceNotifications]
laceext         = "builtin:laceNotifications"
timeout_message = "env:TIMEOUT_MSG:Request timed out"
```

Environment variable substitution does **not** apply to `.config` file defaults. Defaults are static and portable.

### Merge order

1. **Base layer:** all key-value pairs from the `.config` file's `[config]` section.
2. **Override layer:** any key in `lace.config [extensions.<name>]` overwrites the corresponding base value. Keys present in the `.config` defaults but absent from `lace.config` retain their default values -- they are not nullified.
3. **Reserved key:** the `laceext` key in `[extensions.<name>]` is stripped before merge and never visible in `config`.

### Accessing config in rules

The merged configuration is available as `config.<key>`:

```
let $mult = config.spike_multiplier
when $stats.count gte config.min_entries
```

Keys absent from both the `.config` defaults and `lace.config` overrides evaluate to `null`.

## Extension variables (runVars)

Extensions persist state across hook invocations within a run by emitting to `result.runVars`. All keys must be prefixed with the extension name:

```
emit result.runVars <- {
  "laceBaseline.stats": $new_stats
}
```

### Namespace rules

- Keys must start with `{extension_name}.` -- e.g. `"laceBaseline.stats"`, `"laceNotifications.suppressedCount"`.
- An emit with an incorrectly prefixed key is a runtime error.
- Extension variables appear in `runVars` alongside `$$var` entries from scripts. The prefix prevents collisions.
- Values can be any JSON-serializable shape: scalar, object, or array.

### Important limitation

An extension **cannot read back its own runVars**. If it needs per-run state, it should:

- Use `let` bindings accumulated within a rule body.
- Use a dedicated array under `result.actions.<key>` and read from there.
- Use the `set` statement in functions for accumulation patterns.

### Reading another extension's variables

An extension that declares a dependency in `require` can read the dependency's runVars:

```toml
[extension]
name    = "rateLimitNotifications"
version = "1.0.0"
require = ["laceNotifications"]
```

```
let $suppressed = require["laceNotifications"]["laceNotifications.suppressedCount"]
when is_null($suppressed)
let $suppressed = 0
```

| Access | Resolves to |
|---|---|
| `require["<dep>"]` | Map of that dependency's current runVars. Returns `null` if the dep has emitted nothing or is not in `require`. |
| `require["<dep>"]["<key>"]` | The value, or `null` if not yet emitted. |

Reads are never-fail -- accessing a non-required extension or a missing key returns `null`. Reads reflect the current run state at the moment the rule evaluates; if the dependency's hooks have not yet fired, reads return `null`.

## Cross-run state via `prev`

Extensions that need state across runs use `result.runVars` for storage and `prev` for retrieval. The backend persists each run's result and passes it as `prev` on the next run.

```
# Read previous run's baseline stats
let $prev_stats = prev.runVars["laceBaseline.stats"]

# Compute updated stats
let $new_stats = accumulate_stats($prev_stats, result.calls, config.spike_action, config.spike_multiplier)

# Emit for next run
emit result.runVars <- {
  "laceBaseline.stats": $new_stats
}
```

This pattern -- read from `prev`, compute, emit to `runVars` -- is the standard way to build rolling aggregates across probe runs.
