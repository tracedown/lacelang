# laceBaseline

Response timing and size baseline monitoring and spike detection for Lace probes. Bundled with every executor as `builtin:laceBaseline`.

## Purpose

`laceBaseline` tracks rolling averages of HTTP response metrics (six timings plus response size) across probe runs. When a metric deviates significantly from its baseline average, the extension dispatches a `structured()` notification via `laceNotifications`.

This extension demonstrates extension composition -- it depends on `laceNotifications` and calls its exposed `pushNotification()` function to emit spike alerts. All spike data flows through `actions.notifications`; the extension does not declare its own action array.

## Activation

```toml
# lace.config
[executor]
extensions = ["laceNotifications", "laceBaseline"]
```

Or per run: `--enable-extension laceNotifications --enable-extension laceBaseline`.

Both extensions must be active. `laceBaseline` declares `require = ["laceNotifications"]` and will fail startup if `laceNotifications` is absent.

## Tracked metrics

Seven fields from the response object -- six timings and the response size. These are the only metrics the extension keeps sums for:

| Metric | Description |
|--------|-------------|
| `responseTimeMs` | Total response time |
| `dnsMs` | DNS resolution |
| `connectMs` | TCP connection |
| `tlsMs` | TLS handshake (0 for plain HTTP) |
| `ttfbMs` | Time to first byte |
| `transferMs` | Body transfer |
| `sizeBytes` | Response body size |

## Stats model

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

The average for any metric is `sums[metric] / count`. The model stores count and sums rather than averages to allow incremental updates without precision loss.

The backend persists the full result after each run and passes it as `prev` on the next run. The extension reads `prev.runVars["laceBaseline.stats"]` for the accumulated baseline and emits updated stats at the end of each run.

## Spike detection

On each successful call (the `detect_spikes_per_call` rule, registered as `on = ["call after laceNotifications"]`):

1. Read baseline stats from `prev.runVars["laceBaseline.stats"]`.
2. If `count < min_entries`, skip detection (not enough data yet).
3. For each tracked metric, compute `average = sum / count`.
4. If `actual > average * spike_multiplier` and `average > 0`, dispatch a `structured()` notification via `laceNotifications.pushNotification()`. A null metric value is skipped.

The `average > 0` guard prevents false positives for metrics that are normally zero (e.g. `tlsMs` for plain HTTP).

### Spike detection rule

```
when call.outcome eq "success"
when not is_null(call.response)
let $stats = prev.runVars["laceBaseline.stats"]
when not is_null($stats)
when not is_null($stats.count)
when $stats.count gte config.min_entries
let $mult = config.spike_multiplier
let $resp = call.response
check_spike($stats, "responseTimeMs", $resp.responseTimeMs, call.index, $mult)
check_spike($stats, "dnsMs", $resp.dnsMs, call.index, $mult)
check_spike($stats, "connectMs", $resp.connectMs, call.index, $mult)
check_spike($stats, "tlsMs", $resp.tlsMs, call.index, $mult)
check_spike($stats, "ttfbMs", $resp.ttfbMs, call.index, $mult)
check_spike($stats, "transferMs", $resp.transferMs, call.index, $mult)
check_spike($stats, "sizeBytes", $resp.sizeBytes, call.index, $mult)
```

## Stats accumulation

At the end of each run (the `update_stats` rule on the `script` hook):

1. Read previous stats from `prev.runVars["laceBaseline.stats"]` (or initialize from zeros).
2. Iterate all calls from the current run.
3. For each successful call with a response, add its metric values to the rolling sums (a null value counts as `0.0`) and increment count.
4. Emit updated stats to `result.runVars["laceBaseline.stats"]`.

The `accumulate_stats` function (params `prev_stats`, `calls`, `spike_action`, `spike_multiplier`) uses the `set` statement for in-loop accumulation. Abridged to one metric -- the bundled body repeats each line for all seven:

```
let $count = is_null(prev_stats) ? 0 : (is_null(prev_stats.count) ? 0 : prev_stats.count)
let $s_rt  = is_null(prev_stats) ? 0.0 : (is_null(prev_stats.sums) ? 0.0 : (is_null(prev_stats.sums.responseTimeMs) ? 0.0 : prev_stats.sums.responseTimeMs))
# ... one initializer per metric

for $call in calls:
  when $call.outcome eq "success":
    when not is_null($call.response):
      let $skip = spike_action eq "skip" ? check_any_spike(prev_stats, $call.response, spike_multiplier) : false
      when not $skip:
        set $count = $count + 1
        set $s_rt  = $s_rt  + (is_null($call.response.responseTimeMs) ? 0.0 : $call.response.responseTimeMs)
        # ... one set per metric

return {
  count: $count,
  sums: {
    responseTimeMs: $s_rt
    # ... one entry per metric
  }
}
```

When `spike_action = "skip"`, calls where any metric would spike are excluded from the rolling average, preventing one-off anomalies from polluting the baseline.

## Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `min_entries` | int | `5` | Minimum data points before spike detection activates |
| `spike_multiplier` | float | `3.0` | Metric must exceed average x multiplier to be a spike |
| `spike_action` | string | `"include"` | `"include"` or `"skip"` -- whether spiking calls are included in the rolling average |

Override in `lace.config`:

```toml
[executor]
extensions = ["laceNotifications", "laceBaseline"]

[extensions.laceBaseline]
min_entries      = 10
spike_multiplier = 2.5
spike_action     = "skip"
```

## Exposed functions

### `check_spike(stats, metric_name, actual, call_index, multiplier)`

Checks a single metric against the baseline and dispatches a notification if the spike threshold is exceeded. Other extensions that `require = ["laceBaseline"]` can call it -- for example to check with a different multiplier:

```
let $stats = prev.runVars["laceBaseline.stats"]
laceBaseline.check_spike($stats, "ttfbMs", call.response?.ttfbMs, call.index, 2.0)
```

`metric_name` must be one of the seven tracked metrics above. The stats only hold sums for those, so any other name (a custom metric) finds no sum and never detects a spike.

Returns `true` if a spike was detected and a notification dispatched, `null` otherwise (including when `actual` or `stats` is null, `count` is below 1, or the average is not positive).

## Notification format

Spike notifications use `structured()` and are emitted into `actions.notifications` with `trigger: "baseline_spike"`:

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

The backend receives these as regular notification events and can format, group, and deliver them alongside other notifications.
