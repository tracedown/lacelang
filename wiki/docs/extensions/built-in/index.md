# Built-in Extensions

Lace ships two built-in extensions with every executor distribution. They are bundled as `.laceext` files and referenced in `lace.config` with the `builtin:` prefix.

## What "built-in" means

Built-in extensions are regular `.laceext` files that use the same rule language and extension system as any author-written extension. The only difference is distribution: they ship alongside the executor binary and can be referenced as `builtin:<name>` instead of a file path.

Built-in extensions are **not active by default**. They must be listed in `lace.config` to take effect:

```toml
[extensions.laceNotifications]
laceext = "builtin:laceNotifications"

[extensions.laceBaseline]
laceext = "builtin:laceBaseline"
```

## Included extensions

### [laceNotifications](notifications.md)

Notification dispatch for assertion failures and timeouts. When a scope, condition, or call fails, this extension emits structured notification events into `result.actions.notifications`. The backend is responsible for delivering these notifications via its configured transport (email, Slack, webhook, etc.).

Key features:

- Registers `notification` and `silentOnRepeat` options on scopes, conditions, and timeouts
- Emits `notification_event` entries with `text`, `template`, or `structured` payloads
- Supports `op_map` for conditional notification selection based on failure type
- Exposes `pushNotification()` for other extensions to inject notifications
- Suppresses repeated alerts with `silentOnRepeat` (default: true)

### [laceBaseline](baseline.md)

Rolling average timing baseline and spike detection. Tracks 7 HTTP response timing metrics across runs and dispatches notifications when a metric deviates significantly from its baseline.

Key features:

- Tracks `responseTimeMs`, `dnsMs`, `connectMs`, `tlsMs`, `ttfbMs`, `transferMs`, `sizeBytes`
- Detects spikes when a metric exceeds `average * spike_multiplier`
- Depends on `laceNotifications` for alert dispatch
- Carries stats across runs via `result.runVars` and `prev`
- Exposes `check_spike()` for custom baseline checks from other extensions
