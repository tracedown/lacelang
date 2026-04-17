# laceNotifications

Notification dispatch extension for Lace. Recommended to be bundled with every executor as
`builtin:laceNotifications`.

When enabled, this extension emits `notification_event` entries into
`result.actions.notifications` whenever assertions fail or calls time out.
The **backend** (the system that invokes the Lace executor) is responsible
for actually delivering notifications — Lace provides the interface, not
the transport.

---

## Notification types

There are two related type definitions:

- **`notification_val`** — the output type. Appears in
  `result.actions.notifications`. Only `text`, `template`, and
  `structured` are valid.
- **`notification_expr`** — the scripting-time superset. Used in
  scope/condition `notification` options. Includes everything in
  `notification_val` plus `op_map`, which is an expression builder
  that the extension resolves to a concrete `notification_val` before
  emitting. **`op_map` never appears in the result.**

### Output types (`notification_val`)

These are the only types that appear in `result.actions.notifications`.

#### `text(value)`

A literal message string. May contain `$var` and `$$var` references that
the **backend** resolves against its own context (e.g. the script's
declared variables, run metadata, or platform-specific fields). Lace
does **not** perform any interpolation on `text()` values — they are
passed through verbatim to the result.

```
notification: text("Status check failed for $url")
```

**Use case:** Simple human-readable messages. The backend may display them
directly or apply its own formatting. Variable references are optional
and backend-defined.

#### `template(name)`

A reference to a named notification template registered in the backend.
The backend looks up the template by name and renders it using whatever
data it has available (the full `ProbeResult`, platform context, etc.).

```
notification: template("high-severity-alert")
```

**Use case:** When the backend has a library of pre-defined notification
formats (e.g. Slack blocks, email templates, PagerDuty payloads) and
the script author selects one by name.

#### `structured(data)`

A machine-readable object carrying typed failure details. The backend
receives the raw data and formats it however it chooses — no string
composition happens at the Lace level.

```
notification: structured({
  scope:    scope.name,
  op:       scope.op,
  expected: scope.value,
  actual:   scope.actual
})
```

**Use case:** Default notifications emitted by the extension when no
custom `notification` option is set on a scope or condition. The
extension fills in the actual runtime values (scope name, expected
value, actual value, operator) and the backend decides how to present
them.

This is the preferred type for programmatic notification generation
because:

- The backend can aggregate multiple `structured` notifications from
  the same call into a single human-readable message (e.g. grouping
  all failed `expect` scopes).
- No string concatenation or type coercion is needed at the DSL level.
- The backend can localize, format, or template the data freely.

### Scripting-only expression builder (`notification_expr`)

The following type is valid only in script-level `notification` options
(on scopes, conditions, and timeouts). It is resolved by the extension
at evaluation time and **never appears in the result**.

#### `op_map(ops)`

A map from comparison-outcome keys to `notification_expr` values. When
an assertion fails, the extension determines the relationship between
actual and expected (`"lt"`, `"gt"`, `"eq"`, `"neq"`), looks up the
matching entry, and emits the resolved `notification_val`. An optional
`"default"` key serves as a fallback when no key matches.

```
notification: op_map({
  "lt": text("Value is below threshold"),
  "gt": text("Value exceeds threshold"),
  "default": text("Value mismatch")
})
```

**Use case:** Different notification messages depending on *how* the
assertion failed (e.g. "response too slow" vs "response too fast").

Since `op_map` values are themselves `notification_expr`, they can be
nested — though in practice a single level mapping to `text()` or
`structured()` values covers all common cases.

---

## How notifications are emitted

The extension registers rules on the `expect`, `check`, `assert`, and
`call` hooks. On each hook fire:

1. If the scope/condition **passed**, no notification is emitted.
2. If it **failed** and a custom `notification` option is set on the
   scope or condition, that value is used (after `op_map` resolution
   if applicable).
3. If it **failed** and no custom `notification` is set, the extension
   emits a default `structured()` notification containing the failure
   details.
4. For **timeouts**, the extension emits a `text()` notification using
   `config.timeout_message`.

The `silentOnRepeat` option (default `true`) suppresses notifications
when the same scope or condition also failed in the previous run. This
prevents alert storms on persistent failures.

---

## Exposed function: `pushNotification(event)`

Other extensions that `require = ["laceNotifications"]` can call
`laceNotifications.pushNotification({...})` to inject their own
notification events into `result.actions.notifications`. The caller
provides a fully formed `notification_event` object.

---

## Configuration

See `laceNotifications.config` for defaults. Override in `lace.config`:

```toml
[extensions.laceNotifications]
timeout_message = "Custom timeout message for $url"
```

---

## Backend responsibilities

The backend receives `result.actions.notifications` — an array of
`notification_event` objects. Each has:

| Field | Type | Description |
|---|---|---|
| `callIndex` | int | Which call triggered the notification (-1 for script-level) |
| `conditionIndex` | int | Condition index within `.assert()` (-1 for scope-level) |
| `trigger` | string | `"expect"`, `"check"`, `"assert"`, or `"timeout"` |
| `scope` | string? | Scope name for scope-level failures (null for assert/timeout) |
| `notification` | notification_val | The resolved notification — always `text`, `template`, or `structured` (never `op_map`) |

The backend should:

1. **Group** notifications by `callIndex` and `trigger` if it wants
   aggregated messages (e.g. "Call 0: expect failed — status: 200 vs
   503; dnsMs: 240 vs 100").
2. **Resolve** `template()` references against its template library.
3. **Interpolate** `$var`/`$$var` references in `text()` values if
   applicable.
4. **Format** `structured()` data into human-readable messages using
   its own formatting rules.
5. **Deliver** via its configured transport (email, Slack, webhook,
   PagerDuty, etc.).
