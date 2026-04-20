# Actions

The top-level `actions` object in a ProbeResult carries data that the backend should
act on after the probe run completes. It is always present, even when empty.

---

## actions.variables

The `variables` key holds write-back values produced by `.store()` calls in the Lace
script. This is the only typed, mandatory section of `actions`.

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

### Empty variables

When no `.store()` calls executed (or none had write-back targets), the object is
empty:

```json
{
  "actions": {
    "variables": {}
  }
}
```

---

## Extension-defined action arrays

Extensions may add their own keys to the `actions` object. Each extension-defined key
contains an array of action items that the backend should process.

The structure of each action item is defined by the extension, not by the core spec.
The core executor passes these through without interpretation.

### Example: laceNotifications extension

An extension called `laceNotifications` might produce alert actions:

=== "Compact"

    ```json
    {
      "actions": {
        "variables": {
          "lastStatus": 503
        },
        "notifications": [
          {
            "channel": "slack",
            "message": "Health check returned 503"
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
            "channel": "slack",
            "severity": "critical",
            "message": "Health check returned 503"
          },
          {
            "channel": "email",
            "severity": "warning",
            "message": "Response time exceeded 2000ms"
          }
        ]
      }
    }
    ```

The `notifications` array here is entirely defined by the `laceNotifications` extension.
The core executor creates the array and populates it based on extension rules, but the
schema of each item is opaque to core.

### Conventions

- Extension action keys are arrays (never scalar values or plain objects).
- The `variables` key is reserved for the core write-back mechanism and must not be
  used by extensions.
- Extensions that need to emit scalar state should use `runVars` with a
  `{extension_name}.` prefix instead.

---

## Full example

A complete `actions` object from a run that stored variables and triggered an
extension-defined notification:

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
          "responseCode": 503,
          "cursor": null
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
        "responseCode": 503,
        "laceNotifications.lastAlertTime": "2024-01-15T14:23:03.891Z"
      },
      "calls": [
        "..."
      ],
      "actions": {
        "variables": {
          "responseCode": 503,
          "cursor": null
        },
        "notifications": [
          {
            "channel": "slack",
            "severity": "critical",
            "message": "Health check returned 503"
          }
        ]
      }
    }
    ```

Note that `runVars` contains both core `$$var` values (`responseCode`) and
extension-namespaced values (`laceNotifications.lastAlertTime`), while `actions` contains
the write-back variables and the extension action arrays.
