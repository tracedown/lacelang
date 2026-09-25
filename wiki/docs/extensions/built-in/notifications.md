# laceNotifications

Notification dispatch extension for Lace. Bundled with every executor as `builtin:laceNotifications`.

When enabled, this extension emits `notification_event` entries into `result.actions.notifications` whenever assertions fail, calls time out, or a call enters a connection-level error state (DNS failure, refused connection, TLS error). The **backend** (the system that invokes the Lace executor) is responsible for actually delivering notifications -- Lace provides the interface, not the transport.

## Activation

```toml
# lace.config
[executor]
extensions = ["laceNotifications"]
```

Or per run: `lacelang-executor run script.lace --enable-extension laceNotifications`.

## Notification type system

There are two related type definitions:

- **`notification_val`** -- the output type. Only `text`, `template`, and `structured` are valid in the result.
- **`notification_expr`** -- the scripting-time superset. Used in scope, condition and timeout `notification` options. Includes everything in `notification_val` plus `op_map`, which resolves to a concrete `notification_val` before emission. **`op_map` never appears in the result.**

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

A map from keys to `notification_expr` values. When an assertion fails, the extension resolves the map in this order and emits the first match:

1. **The literal actual value** as a key -- `"404"`, `"503"` (scope assertions only).
2. **The relationship** between actual and expected as reported by [`compare()`](../functions.md#primitives) -- `"lt"`, `"eq"`, `"gt"`, or `"neq"`. `compare()` never returns `"lte"` / `"gte"`, so keys like those never match.
3. **`"default"`**.

If nothing matches, no notification is emitted for that failure. Assert conditions skip step 1: they resolve `compare(actualLhs, actualRhs)` and then `"default"`.

```lace
get("https://api.example.com/health")
.expect(status: { value: 200, options: {
  notification: op_map({
    "404": template("endpoint-missing"),
    "gt": text("Server error"),
    "default": text("Unexpected status")
  })
} })
.check(totalDelayMs: { value: 500, options: {
  notification: op_map({
    "gt": text("Response too slow"),
    "default": text("Unexpected timing result")
  })
} })
```

A 404 matches the literal `"404"` key; a 503 has no literal key, `compare(503, 200)` is `"gt"`, so "Server error" is emitted.

**Bare-map shorthand.** A bare object literal in a `notification` option is accepted as shorthand for `op_map({ ... })` and resolves identically:

```lace
get("https://api.example.com/health")
.expect(status: { value: 200, options: {
  notification: { "404": template("endpoint-missing"), "default": text("Unexpected status") }
} })
```

`op_map({ ... })` stays the canonical documented form.

Use case: different messages depending on *how* the assertion failed (e.g. "response too slow" vs "response too fast").

`op_map` values are themselves `notification_expr`, so they can nest -- though in practice a single level mapping to `text()` or `structured()` covers all common cases.

## How notifications are emitted

The extension registers rules on the `expect`, `check`, `assert`, and `call` hooks:

1. If the scope/condition **passed**, no notification is emitted.
2. If it **failed** and a custom `notification` option is set, that value is used -- `text` / `template` / `structured` as-is, `op_map` (or a bare map) resolved as above.
3. If it **failed** and no custom `notification` is set, the extension emits a default `structured()` notification with the failure details: `{ scope, op, expected, actual }` for scopes, `{ kind, expression, actualLhs, actualRhs }` for assert conditions.
4. For **timeouts** (trigger `"timeout"`), the extension emits the call's `timeout.notification` when the script declares one, otherwise a `text()` notification using `config.timeout_message`. See [Timeout notifications](#timeout-notifications).
5. For **connection-level errors** (trigger `"error"`) -- anything that sets `call.error` on a `failure` outcome without a timeout -- the extension emits `structured({ error: call.error })` when the same call had no error in the previous run (or there is no previous run), and stays silent while the error persists.

The assertion rules run on the `expect` / `check` / `assert` hooks, the timeout and error rules on `call`. After a failed `.expect()`, the same call's `.check()` and `.assert()` are still evaluated, so they can still produce notifications on that call.

### Rule example: default expect notification

The `scope_expect_default_notifications` rule:

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

### Rule example: connection error notification

The `call_error_default_notifications` rule:

```
when call.outcome eq "failure"
when not is_null(call.error)
let $prev_error = prev?.calls[call.index]?.error
when is_null($prev_error)
emit result.actions.notifications <- {
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "error",
  notification:    structured({
    error: call.error
  })
}
```

## Timeout notifications

A script can name the notification for a timed-out call on the call's `timeout {}` block:

```lace
get("https://api.example.com/login", {
  timeout: {
    ms: 500,
    action: "warn",
    notification: text("Login endpoint timed out after 500ms")
  }
})
.expect(status: 200)
```

The value is recorded under `config.timeout.extensions.notification` and the timeout rules read it as `call.config.timeout?.extensions?.notification`. A concrete `text` / `template` / `structured` value is emitted as-is; a map form (`op_map` or bare map) has no actual/expected pair to compare, so it resolves to its `"default"` entry. Without a declared notification, the default is `text(config.timeout_message)`.

Timeout notifications fire on every timed-out run -- `silentOnRepeat` does not apply to them (it is not registered on `timeout`).

## silentOnRepeat

The `silentOnRepeat` option suppresses a notification when the same scope or condition also failed in the previous run. This prevents alert storms on persistent failures.

It defaults to `true` **whether or not** the scope has an `options {}` block: `options: { notification: text("...") }` without the key behaves exactly like no options at all. Only an explicit `silentOnRepeat: false` turns suppression off. (The schema's `default = "true"` is documentation only; `is_silent` applies the default itself.)

```toml
[functions.is_silent]
params = ["options", "prev_outcome"]
body   = """
when is_null(options)
return prev_outcome eq "failed"

when options.silentOnRepeat eq false
return false

return prev_outcome eq "failed"
"""
```

The rules look up the matching record in the previous run's `prev.calls[call.index].assertions`:

- **Scopes** match by scope name: `assertions[? $.scope eq scope.name]`.
- **Assert conditions** match by `(method, index)`: `assertions[? $.method eq "assert" and $.index eq condition.index]`. Scope records that precede the conditions in `assertions[]` therefore do not shift the lookup.

```
let $prev_a = prev?.calls[call.index]?.assertions[? $.method eq "assert" and $.index eq condition.index]
let $silent = is_silent(condition.options, $prev_a?.outcome)
when not $silent
# emit notification
```

To disable suppression for a specific scope or condition, set `silentOnRepeat: false` in its `options {}`:

```lace
get("https://api.example.com/health")
.expect(status: { value: 200, options: { silentOnRepeat: false } })
.assert({
  expect: [
    { condition: this.body.healthy eq true, options: { silentOnRepeat: false } }
  ]
})
```

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
[extension]
name    = "laceNotifications"
version = "1.1.0"

[config]
timeout_message = "Request timed out"
```

Override in `lace.config`:

```toml
[executor]
extensions = ["laceNotifications"]

[extensions.laceNotifications]
timeout_message = "Custom timeout message for $url"
```

## Backend responsibilities

The backend receives `result.actions.notifications` -- an array of `notification_event` objects:

| Field | Type | Description |
|---|---|---|
| `callIndex` | int | Which call triggered the notification (-1 for script-level) |
| `conditionIndex` | int | Condition index within `.assert()` (-1 for scope-level) |
| `trigger` | string | `"expect"`, `"check"`, `"assert"`, `"timeout"`, or `"error"` from this extension; peers add their own (`"recovered"` from [laceEmitRecovery](emit-recovery.md), `"baseline_spike"` from [laceBaseline](baseline.md)) |
| `scope` | string? | Scope name for scope-level failures; absent on assert/timeout/error events (peers may set it to `null`) |
| `notification` | notification_val | Always `text`, `template`, or `structured` (never `op_map`) |

The backend should:

1. **Group** notifications by `callIndex` and `trigger` for aggregated messages.
2. **Resolve** `template()` references against its template library.
3. **Interpolate** `$var`/`$$var` references in `text()` values if applicable.
4. **Format** `structured()` data into human-readable messages.
5. **Deliver** via the configured transport (email, Slack, webhook, PagerDuty, etc.).
