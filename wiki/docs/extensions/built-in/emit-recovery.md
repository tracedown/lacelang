# laceEmitRecovery

Recovery notification extension for Lace. Bundled with every executor as `builtin:laceEmitRecovery`.

When enabled, this extension emits a notification when a probe transitions from a failing state (failure or timeout) back to success. It complements `laceNotifications`: while `laceNotifications` handles the "went down" direction (assertion failure notifications with `silentOnRepeat` suppression), `laceEmitRecovery` handles the "came back up" direction.

## Activation

```toml
# lace.config
[executor]
extensions = ["laceNotifications", "laceEmitRecovery"]
```

Or per run: `--enable-extension laceNotifications --enable-extension laceEmitRecovery`.

Both extensions must be active. `laceEmitRecovery` declares `require = ["laceNotifications"]` and will fail startup if `laceNotifications` is absent.

## Behavior

The extension (version 1.1.0) has three rules:

| Rule | Hook | What it does |
|------|------|--------------|
| `capture_recovery_scope` | `expect`, `check` | Records a script-declared `recovery.notification` from the scope's `options` into `runVars["laceEmitRecovery.recoveryNotification"]` |
| `capture_recovery_condition` | `assert` | The same for assert conditions |
| `emit_recovery` | `script` | Detects the recovery transition and pushes the notification |

The capture rules run on every run that declares a `recovery` option -- not only on recovery runs -- so the runVar appears whenever the script declares one. When several scopes or conditions declare one, the last evaluated declaration wins.

`emit_recovery` fires on the `script` hook (after all calls complete and the result outcome is finalized):

1. **Skip if no previous result** -- first runs have nothing to compare against.
2. **Check previous outcome** -- only proceeds if `prev.outcome` was `"failure"` or `"timeout"`.
3. **Check current outcome** -- only proceeds if `result.outcome` is `"success"`.
4. **Pick the notification** -- script-declared `recovery` > config `notification` > `text(config.recovery_message)`.
5. **Emit notification** -- dispatches via `laceNotifications.pushNotification()`.

### Capture rule

`capture_recovery_scope` (`capture_recovery_condition` reads `condition.options` instead):

```
let $r = scope.options?.recovery?.notification
when not is_null($r)
let $val = type_of($r) eq "string" ? text($r) : $r
emit result.runVars <- { "laceEmitRecovery.recoveryNotification": $val }
```

### Recovery detection rule

`emit_recovery` reads the captured value back from `result.runVars` -- `on script` is the one hook where an extension can see its own runVars:

```
when not is_null(prev)
when not is_null(prev.outcome)
let $was_down = prev.outcome eq "failure" or prev.outcome eq "timeout"
when $was_down
when result.outcome eq "success"
let $declared = map_get(result.runVars, "laceEmitRecovery.recoveryNotification")
let $fallback = is_null(config.notification) ? text(config.recovery_message) : config.notification
let $notif = is_null($declared) ? $fallback : $declared
laceNotifications.pushNotification({
  callIndex:      -1,
  conditionIndex: -1,
  trigger:        "recovered",
  scope:          null,
  notification:   $notif
})
```

## When notifications fire

| Previous outcome | Current outcome | Notification? |
|-----------------|-----------------|---------------|
| *(null -- first run)* | success | No |
| *(null -- first run)* | failure | No |
| success | success | No |
| success | failure | No (handled by laceNotifications) |
| failure | failure | No |
| failure | success | **Yes -- recovered** |
| timeout | success | **Yes -- recovered** |
| timeout | failure | No |
| failure | timeout | No |
| timeout | timeout | No |

## Notification format

Recovery notifications use `text(config.recovery_message)` by default and are emitted into `actions.notifications` with `trigger: "recovered"`:

```json
{
  "callIndex": -1,
  "conditionIndex": -1,
  "trigger": "recovered",
  "scope": null,
  "notification": {
    "tag": "text",
    "value": "Service recovered"
  }
}
```

The `callIndex` is always `-1` because recovery is a script-level event, not tied to a specific call. The `trigger` field is `"recovered"` -- backends can key on this for special handling (e.g., including downtime duration from their own state tracking).

## Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `recovery_message` | string | `"Service recovered"` | Default text used when no custom `notification` is set |
| `notification` | notification_val | *(unset)* | Optional override, in TOML table form: `{ tag = "template", name = "..." }` or `{ tag = "text", value = "..." }` |

Default config (`laceEmitRecovery.config`):

```toml
[extension]
name    = "laceEmitRecovery"
version = "1.1.0"

[config]
recovery_message = "Service recovered"
```

Override in `lace.config`:

```toml
[executor]
extensions = ["laceNotifications", "laceEmitRecovery"]

[extensions.laceEmitRecovery]
recovery_message = "Probe is healthy again"
```

Or use a named template. TOML has no function-call syntax, so a tagged notification value is written as a table -- `template("recovery-alert")` becomes:

```toml
[extensions.laceEmitRecovery]
notification = { tag = "template", name = "recovery-alert" }
```

## Script-declared recovery

The script itself may name the recovery notification, on any expect/check
scope or assert condition, via the `recovery` option:

```lace
get("$BASE_URL/health")
.expect(status: { value: 200, options: {
  notification: template("went-down"),
  recovery: { notification: template("back-up") }
} })
```

`recovery.notification` takes a concrete notification value
(`template(...)`, `text(...)`, `structured(...)`); a bare string is
shorthand for `text(...)`. It is emitted as-is -- the `recovery` option is
typed `any`, so an `op_map` or bare map is **not** resolved here.

Precedence for the recovery message:

1. script-declared `recovery` option (when several scopes declare one, the
   last evaluated declaration wins),
2. the extension config `notification` value,
3. the extension config `recovery_message` text.

The declared value is also surfaced in the run's `runVars` as
`laceEmitRecovery.recoveryNotification` -- on every run that declares it,
whether or not the run is a recovery.

## Backend responsibilities

The backend receives recovery notifications as regular entries in `result.actions.notifications`. It should:

1. **Detect** the `"recovered"` trigger to distinguish recovery events from failure notifications.
2. **Correlate** with the previous failure -- the backend has access to `prev` and can compute downtime duration from `prev.startedAt` to `result.startedAt`.
3. **Deliver** via the configured transport, potentially with different formatting or routing than failure alerts (e.g., "all clear" messages to the same channel that received the initial alert).

## Interaction with laceNotifications

The two extensions work together to provide a complete notification lifecycle:

| Event | Extension | Trigger |
|-------|-----------|---------|
| First failure | laceNotifications | `"expect"`, `"check"`, `"assert"`, `"timeout"`, or `"error"` |
| Repeated failure | laceNotifications | *(assertion failures suppressed by `silentOnRepeat`; `"error"` silent while the error persists; `"timeout"` fires every run)* |
| Recovery | laceEmitRecovery | `"recovered"` |
| Stable success | *(neither)* | *(no notification)* |

This mirrors the alerting model of monitoring systems: alert on the initial failure, suppress noise during persistent outages, and notify again when service is restored.
