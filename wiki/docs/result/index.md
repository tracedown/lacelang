# Result Format

Every Lace executor returns a **ProbeResult** -- a single JSON object that captures the
complete outcome of a probe run: timing, per-call details, assertion verdicts, and
write-back actions.

See the [Result Schema reference](../reference/result-schema.md) for the complete,
field-by-field definition.

---

## Top-level fields

These seven fields are **required** and always present, with one exception: an executor declaring `omit: actions` ([conformance levels](../implementers/conformance-levels.md)) never emits `actions`. An optional eighth field, `validationWarnings`, appears only when pre-execution validation produced warnings.

| Field | Type | Description |
|---|---|---|
| `outcome` | `string` | Overall run result: `"success"`, `"failure"`, or `"timeout"`. |
| `startedAt` | `string` (ISO 8601) | UTC timestamp taken before the first call begins. |
| `endedAt` | `string` (ISO 8601) | UTC timestamp taken after all chain methods complete or the failure cascade stops. |
| `elapsedMs` | `integer` | Wall-clock elapsed time in milliseconds (`endedAt - startedAt`). |
| `runVars` | `object` | Final state of all `$$var` assignments. Each key appears at most once (write-once rule). Extension-emitted variables use a `{extension_name}.` prefix. |
| `calls` | `array` | Ordered [call records](call-record.md), including skipped calls. |
| `actions` | `object` | Action map. `actions.variables` is present when the script has write-back `.store()` targets; extensions may add additional arrays. `{}` when there is nothing to report. See [Actions](actions.md). |
| `validationWarnings` | `array` | *Optional.* Structured validator diagnostics (`code`, `callIndex`, `chainMethod`) from pre-execution validation. Present only when the validator produced warnings. Separate from each call's `warnings` strings. |

---

## Outcome values

| Value | Meaning |
|---|---|
| `"success"` | Every call completed and no hard assertion (`.expect()` or `.assert({ expect })`) failed. |
| `"failure"` | At least one hard assertion failed or a non-assertion error occurred (connection refused, TLS error, redirect limit exceeded). An oversize body is a failed `bodySize` assertion, not an error. |
| `"timeout"` | A call timed out (its `timeout.ms` elapsed) and hard-failed the run. There is no run-level timeout. |

---

## Minimal complete example

A probe with a single GET call, one status assertion, and no stored variables:

=== "Compact"

    ```json
    {
      "outcome": "success",
      "startedAt": "2024-01-15T14:23:01.234Z",
      "endedAt": "2024-01-15T14:23:02.891Z",
      "elapsedMs": 1657,
      "runVars": {},
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "request": {
            "url": "https://api.example.com/health",
            "method": "get"
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "responseTimeMs": 145
          },
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "outcome": "passed"
            }
          ]
        }
      ],
      "actions": {}
    }
    ```

=== "Full"

    ```json
    {
      "outcome": "success",
      "startedAt": "2024-01-15T14:23:01.234Z",
      "endedAt": "2024-01-15T14:23:02.891Z",
      "elapsedMs": 1657,
      "runVars": {},
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "startedAt": "2024-01-15T14:23:01.234Z",
          "endedAt": "2024-01-15T14:23:02.891Z",
          "request": {
            "url": "https://api.example.com/health",
            "method": "get",
            "headers": {
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "content-type": "application/json",
              "x-request-id": "req-78f3a"
            },
            "bodyPath": null,
            "bodyNotCapturedReason": "notRequested",
            "responseTimeMs": 145,
            "dnsMs": 12,
            "connectMs": 34,
            "tlsMs": 28,
            "ttfbMs": 98,
            "transferMs": 47,
            "sizeBytes": 256,
            "dns": {
              "resolvedIps": [
                "93.184.216.34"
              ],
              "resolvedIp": "93.184.216.34"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "h2",
              "certificate": {
                "subject": {
                  "cn": "api.example.com"
                },
                "subjectAltNames": [
                  "DNS:api.example.com"
                ],
                "issuer": {
                  "cn": "R3"
                },
                "notBefore": "2024-01-01T00:00:00Z",
                "notAfter": "2024-07-01T00:00:00Z"
              }
            }
          },
          "redirects": [],
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "op": "eq",
              "outcome": "passed",
              "actual": 200,
              "expected": 200,
              "options": null
            }
          ],
          "config": {
            "timeout": {
              "ms": 30000,
              "action": "fail",
              "retries": 0
            },
            "redirects": {
              "follow": true,
              "max": 10
            },
            "security": {
              "rejectInvalidCerts": true
            }
          },
          "warnings": [],
          "error": null
        }
      ],
      "actions": {}
    }
    ```

---

## Sub-pages

| Page | Contents |
|---|---|
| [Call Record](call-record.md) | Per-call fields, request/response records, assertions. |
| [Response Metadata](response-metadata.md) | DNS, TLS, redirect tracking, and timing breakdown. |
| [Body Storage](body-storage.md) | How response bodies are saved as files when enabled. |
| [Actions](actions.md) | Write-back variables and extension-defined action arrays. |
