# laceBaseline

Response timing baseline monitoring and spike detection for Lace probes.

## Purpose

`laceBaseline` tracks rolling averages of HTTP response timing metrics across probe runs. When a metric deviates significantly from its baseline average, the extension dispatches a `structured()` notification via `laceNotifications`.

This extension serves as the reference example of extension composition — it depends on `laceNotifications` and calls its exposed `pushNotification()` function to emit spike alerts. All spike data flows through `actions.notifications` — the extension does not declare its own action array.

## Dependency

```toml
require = ["laceNotifications"]
```

## Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `min_entries` | int | `5` | Minimum data points before spike detection activates |
| `spike_multiplier` | float | `3.0` | Metric must exceed average x multiplier to be a spike |
| `spike_action` | string | `"include"` | `"include"` or `"skip"` — whether spiking calls are included in the rolling average update |

Override in `lace.config`:

```toml
[extensions.laceBaseline]
min_entries      = 10
spike_multiplier = 2.5
spike_action     = "skip"
```

## Tracked Metrics

All timing fields from the response object:

| Metric | Description |
|--------|-------------|
| `responseTimeMs` | Total response time |
| `dnsMs` | DNS resolution |
| `connectMs` | TCP connection |
| `tlsMs` | TLS handshake (0 for plain HTTP) |
| `ttfbMs` | Time to first byte |
| `transferMs` | Body transfer |
| `sizeBytes` | Response body size |

## Stats Model

Stats are stored in `result.runVars["laceBaseline.stats"]` and carried across runs via the `prev` mechanism:

```json
{
  "count": 10,
  "sums": {
    "responseTimeMs": 1450.0,
    "dnsMs": 120.0,
    "connectMs": 340.0,
    "tlsMs": 280.0,
    "ttfbMs": 980.0,
    "transferMs": 470.0,
    "sizeBytes": 51200.0
  }
}
```

Average for any metric = `sums[metric] / count`.

The backend persists the full result after each run and passes it as `prev` on the next run. The extension reads `prev.runVars["laceBaseline.stats"]` for the accumulated baseline and emits updated stats at the end of each run.

## Spike Detection

On each successful call (`on call` hook):

1. Read baseline stats from `prev.runVars["laceBaseline.stats"]`
2. If `count < min_entries`, skip detection (not enough data yet)
3. For each tracked metric, compute `average = sum / count`
4. If `actual > average * spike_multiplier` and `average > 0`, dispatch a `structured()` notification via `laceNotifications.pushNotification()`

The `average > 0` guard prevents false positives for metrics that are normally zero (e.g., `tlsMs` for plain HTTP connections).

## Stats Accumulation

At the end of each run (`on script` hook):

1. Read previous stats from `prev.runVars["laceBaseline.stats"]` (or initialize from zeros)
2. Iterate all calls from the current run
3. For each successful call with a response, add its timing values to the rolling sums and increment count
4. Emit updated stats to `result.runVars["laceBaseline.stats"]`

When `spike_action = "skip"`, calls where any metric would spike are excluded from the rolling average. The extension re-checks thresholds internally (via the pure `check_any_spike` function) — it does not read back from dispatched notifications. The backend stores the stats and processes the notifications; the extension owns the full stats computation.

## Exposed Functions

### `check_spike(stats, metric_name, actual, call_index, multiplier)`

Checks a single metric against the baseline and dispatches a notification if triggered. Other extensions that `require` laceBaseline can call this to perform custom spike checks against the accumulated baseline.

## Notification Format

Spike notifications use the `structured()` type and are emitted into `actions.notifications` with `trigger: "baseline_spike"`:

```json
{
  "callIndex": 0,
  "conditionIndex": -1,
  "trigger": "baseline_spike",
  "scope": "responseTimeMs",
  "notification": {
    "tag": "structured",
    "data": {
      "metric": "responseTimeMs",
      "actual": 1500,
      "average": 145.0,
      "threshold": 435.0,
      "multiplier": 3.0
    }
  }
}
```
