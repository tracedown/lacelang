# laceNotifications

Notification dispatch extension for Lace. Bundled with every executor as `builtin:laceNotifications`.

When enabled, this extension emits `notification_event` entries into `result.actions.notifications` whenever assertions fail or calls time out. The **backend** (the system that invokes the Lace executor) is responsible for actually delivering notifications -- Lace provides the interface, not the transport.

## Activation

```toml
# lace.config
[extensions.laceNotifications]
laceext = "builtin:laceNotifications"
```

## Notification type system

There are two related type definitions:

- **`notification_val`** -- the output type. Only `text`, `template`, and `structured` are valid in the result.
- **`notification_expr`** -- the scripting-time superset. Used in scope/condition `notification` options. Includes everything in `notification_val` plus `op_map`, which resolves to a concrete `notification_val` before emission. **`op_map` never appears in the result.**

### Output types (notification_val)

#### `text(value)`

A literal message string. May contain `$var` and `$$var` references that the **backend** resolves (Lace passes them through verbatim).

```
notification: text("Status check failed for $url")
```

Use case: simple human-readable messages.

#### `template(name)`

A reference to a named notification template registered in the backend. The backend renders it using the full `ProbeResult` and its own context.

```
notification: template("high-severity-alert")
```

Use case: selecting from a library of pre-defined formats (Slack blocks, email templates, PagerDuty payloads).

#### `structured(data)`

A machine-readable object carrying typed failure details. The backend receives the raw data and formats it as it chooses.

```
notification: structured({
  scope:    scope.name,
  op:       scope.op,
  expected: scope.value,
  actual:   scope.actual
})
```

Use case: default notifications emitted by the extension when no custom `notification` option is set. The backend can aggregate multiple `structured` notifications from the same call into a single message, localize the output, or apply any formatting.

### Scripting-only expression builder (notification_expr)

#### `op_map(ops)`

A map from comparison-outcome keys to `notification_expr` values. When an assertion fails, the extension determines the relationship between actual and expected (`"lt"`, `"gt"`, `"eq"`, `"neq"`), looks up the matching entry, and emits the resolved `notification_val`. An optional `"default"` key serves as a fallback.

```
notification: op_map({
  "lt": text("Value is below threshold"),
  "gt": text("Value exceeds threshold"),
  "default": text("Value mismatch")
})
```

Use case: different messages depending on *how* the assertion failed (e.g. "response too slow" vs "response too fast").

`op_map` values are themselves `notification_expr`, so they can nest -- though in practice a single level mapping to `text()` or `structured()` covers all common cases.

## How notifications are emitted

The extension registers rules on the `expect`, `check`, `assert`, and `call` hooks:

1. If the scope/condition **passed**, no notification is emitted.
2. If it **failed** and a custom `notification` option is set, that value is used (after `op_map` resolution if applicable).
3. If it **failed** and no custom `notification` is set, the extension emits a default `structured()` notification with the failure details.
4. For **timeouts**, the extension emits a `text()` notification using `config.timeout_message`.

### Rule example: default expect notification

```
when scope.outcome eq "failed"
when is_null(scope.options?.notification)
let $prev_scope = prev?.calls[call.index]?.assertions[? $.scope eq scope.name]
let $silent = is_silent(scope.options, $prev_scope?.outcome)
when not $silent
emit result.actions.notifications <- {
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "expect",
  scope:           scope.name,
  notification:    structured({
    scope:    scope.name,
    op:       scope.op,
    expected: scope.value,
    actual:   scope.actual
  })
}
```

## silentOnRepeat

The `silentOnRepeat` option (default `true`) suppresses notifications when the same scope or condition also failed in the previous run. This prevents alert storms on persistent failures.

The extension checks the previous run's result via `prev` to determine if the same assertion was already failing:

```
let $prev_scope = prev?.calls[call.index]?.assertions[? $.scope eq scope.name]
let $silent = is_silent(scope.options, $prev_scope?.outcome)
when not $silent
// emit notification
```

To disable suppression for a specific scope, set `silentOnRepeat: false` in the scope's `options {}`.

## pushNotification() exposed function

Other extensions that `require = ["laceNotifications"]` can inject their own notification events:

```
laceNotifications.pushNotification({
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "baseline_spike",
  scope:           "responseTimeMs",
  notification:    structured({
    metric:     "responseTimeMs",
    actual:     1500,
    average:    145.0,
    threshold:  435.0,
    multiplier: 3.0
  })
})
```

The function appends the event to `result.actions.notifications` and returns the event object. The emit is attributed to `laceNotifications` since the exposed function executes in the owning extension's context.

## Configuration

Default config (`laceNotifications.config`):

```toml
[config]
timeout_message = "Request timed out"
```

Override in `lace.config`:

```toml
[extensions.laceNotifications]
laceext         = "builtin:laceNotifications"
timeout_message = "Custom timeout message for $url"
```

## Backend responsibilities

The backend receives `result.actions.notifications` -- an array of `notification_event` objects:

| Field | Type | Description |
|---|---|---|
| `callIndex` | int | Which call triggered the notification (-1 for script-level) |
| `conditionIndex` | int | Condition index within `.assert()` (-1 for scope-level) |
| `trigger` | string | `"expect"`, `"check"`, `"assert"`, or `"timeout"` |
| `scope` | string? | Scope name for scope-level failures (null for assert/timeout) |
| `notification` | notification_val | Always `text`, `template`, or `structured` (never `op_map`) |

The backend should:

1. **Group** notifications by `callIndex` and `trigger` for aggregated messages.
2. **Resolve** `template()` references against its template library.
3. **Interpolate** `$var`/`$$var` references in `text()` values if applicable.
4. **Format** `structured()` data into human-readable messages.
5. **Deliver** via the configured transport (email, Slack, webhook, PagerDuty, etc.).
