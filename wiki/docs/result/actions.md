# Actions

The top-level `actions` object in a ProbeResult carries data that the backend should
act on after the probe run completes. It is always present (`{}` when there is nothing to
report), except from executors that declare the `omit: actions`
[conformance level](../implementers/conformance-levels.md), which never emit it.

---

## actions.variables

The `variables` key holds write-back values produced by `.store()` calls in the Lace
script. It is the only typed section of `actions`, and it is present only when the
script has write-back `.store()` targets (`$name` or plain keys). `$$name` keys go to
`runVars` instead.

### How store works

When a Lace script calls `.store()`, it writes values that should be persisted by the
backend for use in future probe runs. These appear in `actions.variables` as a flat
key-value map.

**Key naming:** For `$name` targets in the source script, the `$` prefix is stripped in
the result. Plain keys appear as-is.

| Source `.store()` target | Key in `actions.variables` |
|---|---|
| `$cursor` | `"cursor"` |
| `$lastToken` | `"lastToken"` |
| `last_count` | `"last_count"` |

**Values** may be any JSON-serialisable shape -- strings, numbers, booleans, objects,
arrays, or null.

### Example

Given a Lace script that stores a pagination cursor and a count:

```json
{
  "actions": {
    "variables": {
      "cursor": "eyJwYWdlIjozfQ==",
      "lastCount": 42
    }
  }
}
```

### No write-back targets

When the script has no write-back `.store()` targets, `variables` is absent:

```json
{
  "actions": {}
}
```

A write-back `.store()` on a call that hard-failed is skipped, as are the `.store()`
blocks of every later (skipped) call.

---

## Extension-defined action arrays

Extensions may add their own keys to the `actions` object. Each extension-defined key
contains an array of action items that the backend should process.

The structure of each action item is defined by the extension, not by the core spec.
The core executor passes these through without interpretation.

### Example: laceNotifications extension

The bundled `laceNotifications` extension emits one item per failure into
`actions.notifications`. Each item records where the failure happened and the
notification to send: the script's own `text(...)` / `template(...)` value when the
failing scope or condition declares one, otherwise a `structured` notification carrying
the raw failure data.

=== "Compact"

    ```json
    {
      "actions": {
        "notifications": [
          {
            "callIndex": 0,
            "conditionIndex": -1,
            "trigger": "expect",
            "scope": "status",
            "notification": {
              "tag": "template",
              "name": "not_found_alert"
            }
          }
        ]
      }
    }
    ```

=== "Full"

    ```json
    {
      "actions": {
        "variables": {
          "lastStatus": 503
        },
        "notifications": [
          {
            "callIndex": 0,
            "conditionIndex": -1,
            "trigger": "expect",
            "scope": "status",
            "notification": {
              "tag": "structured",
              "data": {
                "scope": "status",
                "op": "eq",
                "expected": 200,
                "actual": 503
              }
            }
          },
          {
            "callIndex": 0,
            "conditionIndex": -1,
            "trigger": "check",
            "scope": "totalDelayMs",
            "notification": {
              "tag": "text",
              "value": "Response time exceeded 2000ms"
            }
          }
        ]
      }
    }
    ```

| Field | Description |
|---|---|
| `callIndex` | Index of the call that failed. |
| `conditionIndex` | Index of the `.assert()` condition; `-1` for scope failures, timeouts and connection errors. |
| `trigger` | What fired it: `"expect"`, `"check"`, `"assert"`, `"timeout"` or `"error"` (peer extensions add their own, e.g. `"recovered"`, `"baseline_spike"`). |
| `scope` | Scope name for `expect` / `check` failures; absent or `null` for assert, timeout and error events. |
| `notification` | A tagged value: `{ "tag": "text", "value": ... }`, `{ "tag": "template", "name": ... }` or `{ "tag": "structured", "data": {...} }`. |

The `notifications` array here is entirely defined by the `laceNotifications` extension.
The executor creates the array and populates it from extension rules, but the shape of
each item is opaque to core. See the [laceNotifications](../extensions/built-in/notifications.md)
page for the rules that produce it.

### Conventions

- Extension action keys are arrays (never scalar values or plain objects).
- The `variables` key is reserved for the core write-back mechanism and must not be
  used by extensions.
- Extensions that need to emit scalar state should use `runVars` with a
  `{extension_name}.` prefix instead.

---

## Full example

A complete result from a run that stored variables on an earlier call and then failed,
triggering an extension-defined notification:

=== "Compact"

    ```json
    {
      "outcome": "failure",
      "startedAt": "2024-01-15T14:23:01.234Z",
      "endedAt": "2024-01-15T14:23:03.891Z",
      "elapsedMs": 2657,
      "calls": [
        "..."
      ],
      "actions": {
        "variables": {
          "cursor": "eyJwYWdlIjozfQ=="
        }
      }
    }
    ```

=== "Full"

    ```json
    {
      "outcome": "failure",
      "startedAt": "2024-01-15T14:23:01.234Z",
      "endedAt": "2024-01-15T14:23:03.891Z",
      "elapsedMs": 2657,
      "runVars": {
        "token": "abc123"
      },
      "calls": [
        "..."
      ],
      "actions": {
        "variables": {
          "cursor": "eyJwYWdlIjozfQ=="
        },
        "notifications": [
          {
            "callIndex": 1,
            "conditionIndex": -1,
            "trigger": "expect",
            "scope": "status",
            "notification": {
              "tag": "structured",
              "data": {
                "scope": "status",
                "op": "eq",
                "expected": 200,
                "actual": 503
              }
            }
          }
        ]
      }
    }
    ```

Note that `runVars` holds the run-scope `$$var` values (`$$token` -> `token`) -- and any
extension-namespaced values an extension emits, such as
`laceEmitRecovery.recoveryNotification` -- while `actions` holds the write-back variables
(`$cursor` -> `cursor`) and the extension action arrays.
