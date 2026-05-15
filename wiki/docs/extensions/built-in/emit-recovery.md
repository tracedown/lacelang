# laceEmitRecovery

Recovery notification extension for Lace. Bundled with every executor as `builtin:laceEmitRecovery`.

When enabled, this extension emits a notification when a probe transitions from a failing state (failure or timeout) back to success. It complements `laceNotifications`: while `laceNotifications` handles the "went down" direction (assertion failure notifications with `silentOnRepeat` suppression), `laceEmitRecovery` handles the "came back up" direction.

## Activation

```toml
# lace.config
[extensions.laceNotifications]
laceext = "builtin:laceNotifications"

[extensions.laceEmitRecovery]
laceext = "builtin:laceEmitRecovery"
```

Both extensions must be listed. `laceEmitRecovery` declares `require = ["laceNotifications"]` and will fail startup if `laceNotifications` is absent.

## Behavior

The extension fires a single rule on the `script` hook (after all calls complete and the result outcome is finalized):

1. **Skip if no previous result** -- first runs have nothing to compare against.
2. **Check previous outcome** -- only proceeds if `prev.outcome` was `"failure"` or `"timeout"`.
3. **Check current outcome** -- only proceeds if `result.outcome` is `"success"`.
4. **Emit notification** -- dispatches via `laceNotifications.pushNotification()`.

### Recovery detection rule

```
when not is_null(prev)
when not is_null(prev.outcome)
let $was_down = prev.outcome eq "failure" or prev.outcome eq "timeout"
when $was_down
when result.outcome eq "success"
let $notif = is_null(config.notification) ? text(config.recovery_message) : config.notification
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

Recovery notifications use `text()` by default and are emitted into `actions.notifications` with `trigger: "recovered"`:

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
| `notification` | notification_val | *(unset)* | Optional override -- set to `template("name")` or `text("custom message")` |

Override in `lace.config`:

```toml
[extensions.laceEmitRecovery]
laceext          = "builtin:laceEmitRecovery"
recovery_message = "Probe is healthy again"
```

Or use a named template:

```toml
[extensions.laceEmitRecovery]
laceext      = "builtin:laceEmitRecovery"
notification = template("recovery-alert")
```

## Backend responsibilities

The backend receives recovery notifications as regular entries in `result.actions.notifications`. It should:

1. **Detect** the `"recovered"` trigger to distinguish recovery events from failure notifications.
2. **Correlate** with the previous failure -- the backend has access to `prev` and can compute downtime duration from `prev.startedAt` to `result.startedAt`.
3. **Deliver** via the configured transport, potentially with different formatting or routing than failure alerts (e.g., "all clear" messages to the same channel that received the initial alert).

## Interaction with laceNotifications

The two extensions work together to provide a complete notification lifecycle:

| Event | Extension | Trigger |
|-------|-----------|---------|
| First failure | laceNotifications | `"expect"`, `"check"`, `"assert"`, or `"timeout"` |
| Repeated failure | laceNotifications | *(suppressed by `silentOnRepeat`)* |
| Recovery | laceEmitRecovery | `"recovered"` |
| Stable success | *(neither)* | *(no notification)* |

This mirrors the alerting model of monitoring systems: alert on the initial failure, suppress noise during persistent outages, and notify again when service is restored.
