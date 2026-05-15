# laceEmitRecovery

Recovery notification extension for Lace. Emits a notification when a probe
transitions from a failing state (failure or timeout) back to success.

This extension complements `laceNotifications`: while `laceNotifications`
handles the "went down" direction (assertion failure notifications with
`silentOnRepeat` suppression), `laceEmitRecovery` handles the "came back up"
direction.

## Dependency

```toml
require = ["laceNotifications"]
```

## Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `recovery_message` | string | `"Service recovered"` | Default text used when no custom `notification` is set |
| `notification` | notification_val | *(unset)* | Optional override — set to `template("name")` or `text("custom message")` |

Override in `lace.config`:

```toml
[extensions.laceEmitRecovery]
recovery_message = "Probe is healthy again"
```

Or use a named template:

```toml
[extensions.laceEmitRecovery]
notification = template("recovery-alert")
```

## Behavior

The extension fires a single rule on the `script` hook (after all calls
complete and the result outcome is finalized):

1. **Skip if no previous result** — first runs have nothing to compare against.
2. **Check previous outcome** — only proceeds if `prev.outcome` was `"failure"` or `"timeout"`.
3. **Check current outcome** — only proceeds if `result.outcome` is `"success"`.
4. **Emit notification** — dispatches via `laceNotifications.pushNotification()`.

### Emitted notification event

```json
{
  "callIndex": -1,
  "conditionIndex": -1,
  "trigger": "recovered",
  "scope": null,
  "notification": { "tag": "text", "value": "Service recovered" }
}
```

The `trigger` field is `"recovered"` — backends can key on this for special
handling (e.g., including downtime duration from their own state tracking).

## When notifications fire

| Previous outcome | Current outcome | Notification? |
|-----------------|-----------------|---------------|
| *(null — first run)* | success | No |
| *(null — first run)* | failure | No |
| success | success | No |
| success | failure | No (handled by laceNotifications) |
| failure | failure | No |
| failure | success | **Yes — recovered** |
| timeout | success | **Yes — recovered** |
| timeout | failure | No |
| failure | timeout | No |
| timeout | timeout | No |
