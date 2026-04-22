# Result Format

Every Lace executor returns a **ProbeResult** -- a single JSON object that captures the
complete outcome of a probe run: timing, per-call details, assertion verdicts, and
write-back actions.

The schema is defined in
[`schemas/result.json`](https://lacelang.dev/schemas/result/0.9.1) and specified in
lace-spec.md section 9.

---

## Top-level fields

All seven fields are **required** and always present.

| Field | Type | Description |
|---|---|---|
| `outcome` | `string` | Overall run result: `"success"`, `"failure"`, or `"timeout"`. |
| `startedAt` | `string` (ISO 8601) | UTC timestamp taken before the first call begins. |
| `endedAt` | `string` (ISO 8601) | UTC timestamp taken after all chain methods complete or the failure cascade stops. |
| `elapsedMs` | `integer` | Wall-clock elapsed time in milliseconds (`endedAt - startedAt`). |
| `runVars` | `object` | Final state of all `$$var` assignments. Each key appears at most once (write-once rule). Extension-emitted variables use a `{extension_name}.` prefix. |
| `calls` | `array` | Ordered [call records](call-record.md), including skipped calls. |
| `actions` | `object` | Action map. `actions.variables` is always present. Extensions may add additional arrays. See [Actions](actions.md). |

---

## Outcome values

| Value | Meaning |
|---|---|
| `"success"` | Every call completed and all hard assertions (`.expect()`) passed. |
| `"failure"` | At least one hard assertion failed or a non-assertion error occurred (connection refused, body too large, redirect limit exceeded). |
| `"timeout"` | The run exceeded the configured timeout before completing. |

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
      "actions": {
        "variables": {}
      }
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
              "user-agent": "Lace/0.9"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "content-type": "application/json",
              "x-request-id": "req-78f3a"
            },
            "bodyPath": "/probe_runs/abc/call_0_response.json",
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
              "ms": 5000,
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
      "actions": {
        "variables": {}
      }
    }
    ```

---

## Sub-pages

| Page | Contents |
|---|---|
| [Call Record](call-record.md) | Per-call fields, request/response records, assertions. |
| [Response Metadata](response-metadata.md) | DNS, TLS, redirect tracking, and timing breakdown. |
| [Body Storage](body-storage.md) | How request/response bodies are stored as files. |
| [Actions](actions.md) | Write-back variables and extension-defined action arrays. |
