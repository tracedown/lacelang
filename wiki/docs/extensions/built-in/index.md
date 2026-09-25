# Built-in Extensions

Lace ships three built-in extensions with every executor distribution: `laceNotifications`, `laceEmitRecovery` and `laceBaseline`. They are bundled as `.laceext` files; their `[extensions.<name>]` config tables default to `laceext = "builtin:<name>"`, so no file path is needed.

## What "built-in" means

Built-in extensions are regular `.laceext` files that use the same rule language and extension system as any author-written extension. The only difference is distribution: they ship alongside the executor and resolve as `builtin:<name>` instead of a file path.

Built-in extensions are **not active by default**. Activate them in `lace.config` under `[executor] extensions`:

```toml
[executor]
extensions = ["laceNotifications", "laceEmitRecovery", "laceBaseline"]
```

or per run with `--enable-extension NAME` (repeatable):

```bash
lacelang-executor run script.lace \
  --enable-extension laceNotifications \
  --enable-extension laceEmitRecovery
```

An `[extensions.<name>]` table only configures an extension -- on its own (even with `laceext = "builtin:<name>"`) it activates nothing. `laceEmitRecovery` and `laceBaseline` both `require` `laceNotifications`, so it must be active too.

## Included extensions

### [laceNotifications](notifications.md)

Notification dispatch for assertion failures, timeouts and connection errors. When a scope, condition, or call fails, this extension emits notification events into `result.actions.notifications`. The backend is responsible for delivering these notifications via its configured transport (email, Slack, webhook, etc.).

Key features:

- Registers `notification` and `silentOnRepeat` options on scopes and conditions, and `notification` on timeouts
- Emits `notification_event` entries with `text`, `template`, or `structured` payloads -- trigger `expect`, `check`, `assert`, `timeout`, or `error` (entry into a connection-level error state)
- Supports `op_map` (or a bare `{ ... }` map as shorthand) for conditional notification selection based on how the value failed
- Exposes `pushNotification()` for other extensions to inject notifications
- Suppresses repeated alerts with `silentOnRepeat` -- default `true` with or without an `options {}` block; only an explicit `false` turns it off. Timeout notifications are never suppressed.

### [laceEmitRecovery](emit-recovery.md)

Recovery notification on failure-to-success transitions. When the previous run failed or timed out and the current run succeeds, this extension emits a notification with `trigger: "recovered"` via `laceNotifications.pushNotification()`.

Key features:

- Registers a `recovery` option on scopes and conditions, so the script can name its own recovery notification: `options: { recovery: { notification: template("back-up") } }`
- Precedence: script-declared `recovery` > config `notification` (TOML table form, e.g. `{ tag = "template", name = "..." }`) > `text(config.recovery_message)`
- Treats `timeout` the same as `failure` for transition detection
- Skips first runs (no previous result to compare)
- Depends on `laceNotifications` for dispatch

### [laceBaseline](baseline.md)

Rolling-average baseline and spike detection. Tracks 7 HTTP response metrics (six timings plus response size) across runs and dispatches notifications when a metric deviates significantly from its baseline.

Key features:

- Tracks `responseTimeMs`, `dnsMs`, `connectMs`, `tlsMs`, `ttfbMs`, `transferMs`, `sizeBytes`
- Detects spikes when a metric exceeds `average * spike_multiplier`
- Depends on `laceNotifications` for alert dispatch (trigger `baseline_spike`)
- Carries stats across runs via `result.runVars` and `prev`
- Exposes `check_spike()` for peer extensions -- only for the seven tracked metrics
