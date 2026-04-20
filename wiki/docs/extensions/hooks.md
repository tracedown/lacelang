# Hook Points

Rules fire at specific points during script execution. There are 12 hooks organized as 6 before/after pairs.

## Hook pairs

| Before hook | After hook | Fires on |
|---|---|---|
| `on before script` | `on script` | Once per run |
| `on before call` | `on call` | Each HTTP call |
| `on before expect` | `on expect` | Each `.expect()` scope |
| `on before check` | `on check` | Each `.check()` scope |
| `on before assert` | `on assert` | Each `.assert()` condition |
| `on before store` | `on store` | Each `.store()` entry |

`on before` hooks fire before the block executes -- they have no outcome-related fields. `on` hooks fire after and include the results.

## Registering rules on hooks

```toml
[[rules.rule]]
name = "detect_spikes_per_call"
on   = ["call after laceNotifications"]
body = """
when call.outcome eq "success"
when not is_null(call.response)
// ...
"""
```

A rule can register on multiple hooks:

```toml
on = ["expect", "check"]
```

## Context objects by hook

### `on before script` / `on script`

Fires once per run at the outer script boundary.

**`on before script` context:**

| Name | Type | Description |
|---|---|---|
| `script.callCount` | int | Total number of calls declared in the source |
| `script.startedAt` | string | ISO 8601 UTC timestamp when the run began |
| `prev` | object or null | Previous result if `--prev-results` was provided |

**`on script` adds:**

| Name | Type | Description |
|---|---|---|
| `script.endedAt` | string | Timestamp when the last call completed |
| `result.outcome` | string | `"success"`, `"failure"`, or `"timeout"` |
| `result.calls` | array | Finalized call records |
| `result.runVars` | object | Final `runVars` map |
| `result.actions` | object | Accumulated actions from all extensions |

`on script` fires even when the run fails early, so extensions can flush state reliably.

### `on before call` / `on call`

Fires before/after each HTTP call.

**`on before call` context:**

| Name | Type | Description |
|---|---|---|
| `call.index` | int | Zero-based call index |
| `call.request` | object | Resolved request: `url`, `method`, `headers`, `bodyPath` |
| `call.config` | object | Resolved call config including extension fields |
| `prev` | object or null | Previous result |

**`on call` adds:**

| Name | Type | Description |
|---|---|---|
| `call.outcome` | string | `"success"`, `"failure"`, `"timeout"`, or `"skipped"` |
| `call.response` | object or null | Full response or null |
| `call.assertions` | array | All assertion records from this call |

### `on before expect` / `on expect`

Fires before/after each scope in `.expect()`.

**`on before expect` context:**

| Name | Type | Description |
|---|---|---|
| `scope.name` | string | Scope name, e.g. `"status"`, `"totalDelayMs"` |
| `scope.value` | any | Expected value |
| `scope.op` | string | Comparison operator |
| `scope.options` | object or null | The `options {}` object from source |
| `call.index` | int | Call index |
| `this` | object | Current response |
| `prev` | object or null | Previous result |

**`on expect` adds:**

| Name | Type | Description |
|---|---|---|
| `scope.actual` | any | Actual value observed |
| `scope.outcome` | string | `"passed"`, `"failed"`, or `"indeterminate"` |

### `on before check` / `on check`

Same context structure as `on before expect` / `on expect`. Fires for `.check()` scopes.

### `on before assert` / `on assert`

Fires before/after each condition in `.assert()`.

**`on before assert` context:**

| Name | Type | Description |
|---|---|---|
| `condition.index` | int | Index within the assert array |
| `condition.kind` | string | `"expect"` or `"check"` |
| `condition.expression` | string | Expression as written in source |
| `condition.options` | object or null | The `options {}` object |
| `call.index` | int | Call index |
| `this` | object | Current response |
| `prev` | object or null | Previous result |

**`on assert` adds:**

| Name | Type | Description |
|---|---|---|
| `condition.actualLhs` | any | Resolved left operand |
| `condition.actualRhs` | any | Resolved right operand |
| `condition.outcome` | string | `"passed"`, `"failed"`, or `"indeterminate"` |

### `on before store` / `on store`

Fires before/after each entry in `.store()`.

**`on before store` context:**

| Name | Type | Description |
|---|---|---|
| `entry.key` | string | Store key (includes `$`/`$$` prefix if present) |
| `entry.value` | any | Value to be written |
| `entry.scope` | string | `"run"` (for `$$`) or `"writeback"` (for `$` and plain keys) |
| `call.index` | int | Call index |
| `this` | object | Current response |
| `prev` | object or null | Previous result |

**`on store` adds:**

| Name | Type | Description |
|---|---|---|
| `entry.written` | bool | Whether the write succeeded |

## Rule ordering

### Ordering qualifiers

Each `on` entry can include `after` and `before` qualifiers to control execution order:

```toml
# Run after laceNotifications rules at this hook
on = ["check after laceNotifications"]

# Run before laceMetrics rules at this hook
on = ["check before laceMetrics"]

# Both at once
on = ["check after laceNotifications before laceMetrics"]

# Different ordering on different hooks
on = [
  "check after laceNotifications",
  "assert after laceNotifications"
]
```

### Resolution algorithm

At each hook, the executor resolves rule order:

1. **Gather** -- collect all rules registered for this hook.
2. **Expand defaults** -- for each rule in extension B with `require = [A]`, add an implicit `after A` when A has rules on this hook.
3. **Name-resolve** -- every name in `after X` / `before X` must be a loaded extension (not necessarily in `require`).
4. **Drop unfulfillable** -- if a rule says `after X` but X has no rules on this hook, the constraint is silently dropped (and may cascade).
5. **Topological sort** -- the surviving rules form a DAG. A cycle is a startup error.
6. **Execute** -- rules run in topological order. Ties break by declaration order within a file, then by extension name alphabetically.

### Example

`laceBaseline` declares `require = ["laceNotifications"]` and registers:

```toml
on = ["call after laceNotifications"]
```

This means the spike detection rule runs after all `laceNotifications` rules at the `call` hook, ensuring notifications from the base extension are already emitted before baseline checks run.
