# Call Record

Each entry in the top-level `calls` array is a **CallRecord** -- a self-contained
snapshot of one HTTP call and its assertions.

---

## Call-level fields

All fields are required.

| Field | Type | Description |
|---|---|---|
| `index` | `integer` | Zero-based call index in script order. |
| `outcome` | `string` | `"success"`, `"failure"`, `"timeout"`, or `"skipped"`. |
| `startedAt` | `string \| null` | ISO 8601 UTC timestamp. `null` for skipped calls. |
| `endedAt` | `string \| null` | ISO 8601 UTC timestamp. `null` for skipped calls. |
| `request` | `object \| null` | The request that was sent. `null` for skipped calls. |
| `response` | `object \| null` | The response received. `null` for skipped, timeout, or connection failure. |
| `redirects` | `array` | Ordered redirect URLs. See [Response Metadata](response-metadata.md#redirect-tracking). |
| `assertions` | `array` | All evaluated assertions in evaluation order. Empty for skipped calls. |
| `config` | `object` | Resolved call config after defaults are applied. Extensions may add registered fields. |
| `warnings` | `array` | Warning strings (null interpolations, TLS warnings, skipped writes). Empty if none. |
| `error` | `string \| null` | Non-assertion failure detail (connection error, TLS error, redirect limit, body too large). `null` otherwise. |

### Example: successful call

=== "Compact"

    ```json
    {
      "index": 0,
      "outcome": "success",
      "startedAt": "2024-01-15T14:23:01.234Z",
      "endedAt": "2024-01-15T14:23:01.379Z",
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
      ],
      "error": null
    }
    ```

=== "Full"

    ```json
    {
      "index": 0,
      "outcome": "success",
      "startedAt": "2024-01-15T14:23:01.234Z",
      "endedAt": "2024-01-15T14:23:01.379Z",
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
    ```

---

## Request record

| Field | Type | Description |
|---|---|---|
| `url` | `string` | Resolved URL after variable interpolation. |
| `method` | `string` | One of `"get"`, `"post"`, `"put"`, `"patch"`, `"delete"`. Always lower-case. |
| `headers` | `object` | Resolved headers sent on the wire. Keys are header names, values are strings. |

```json
{
  "url": "https://api.example.com/login",
  "method": "post",
  "headers": {
    "content-type": "application/json",
    "authorization": "Bearer tok_abc"
  }
```

---

## Response record

The `response` field is `null` when the call was skipped, timed out before a response
arrived, or suffered a connection failure. When present, all fields are required.

| Field | Type | Description |
|---|---|---|
| `status` | `integer` | HTTP status code (100--599). |
| `statusText` | `string` | HTTP reason phrase (e.g. `"OK"`, `"Not Found"`). |
| `headers` | `object` | Response headers. Keys are **lower-cased**. Single-value headers are strings; multi-value headers are arrays of strings. |
| `bodyPath` | `string \| null` | Absolute path to the response body file. `null` when not captured. See [Body Storage](body-storage.md). |
| `bodyNotCapturedReason` | `string` | Present when `bodyPath` is `null`. One of `"bodyTooLarge"`, `"notRequested"`, `"timeout"`. |
| `responseTimeMs` | `integer` | Total response time in milliseconds. |
| `dnsMs` | `integer` | DNS resolution time in milliseconds. |
| `connectMs` | `integer` | TCP connection time in milliseconds. |
| `tlsMs` | `integer` | TLS handshake time in milliseconds. `0` for plain HTTP. |
| `ttfbMs` | `integer` | Time to first byte in milliseconds. |
| `transferMs` | `integer` | Body transfer time in milliseconds. |
| `sizeBytes` | `integer` | Response body size in bytes. |
| `dns` | `object` | DNS resolution metadata. See [Response Metadata](response-metadata.md#dns-metadata). |
| `tls` | `object \| null` | TLS session metadata. `null` for plain HTTP. See [Response Metadata](response-metadata.md#tls-metadata). |

### Header format

Header names are always lower-cased. A header that appears once is a string; a header
that appears multiple times is an array of strings:

```json
{
  "content-type": "application/json",
  "set-cookie": [
    "session=abc; Path=/; HttpOnly",
    "prefs=dark; Path=/"
  ],
  "x-request-id": "req-12345"
}
```

### Full response example

=== "Compact"

    ```json
    {
      "status": 200,
      "statusText": "OK",
      "bodyPath": "/probe_runs/abc/call_0_response.json",
      "responseTimeMs": 145,
      "sizeBytes": 1024
    }
    ```

=== "Full"

    ```json
    {
      "status": 200,
      "statusText": "OK",
      "headers": {
        "content-type": "application/json",
        "cache-control": "no-store",
        "x-request-id": "req-78f3a",
        "x-ratelimit-remaining": "99"
      },
      "bodyPath": "/probe_runs/abc/call_0_response.json",
      "responseTimeMs": 145,
      "dnsMs": 12,
      "connectMs": 34,
      "tlsMs": 28,
      "ttfbMs": 98,
      "transferMs": 47,
      "sizeBytes": 1024,
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
            "cn": "example.com"
          },
          "subjectAltNames": [
            "DNS:example.com",
            "DNS:www.example.com"
          ],
          "issuer": {
            "cn": "R3"
          },
          "notBefore": "2024-01-01T00:00:00Z",
          "notAfter": "2024-07-01T00:00:00Z"
        }
      }
    }
    ```

---

## Assertions

The `assertions` array contains every assertion evaluated for the call, in evaluation
order. There are two discriminated types, identified by the `method` field.

### ScopeAssertion (`.expect()` / `.check()`)

Produced by `.expect()` and `.check()` chain methods that target a named scope.

| Field | Type | Description |
|---|---|---|
| `method` | `string` | `"expect"` or `"check"`. |
| `scope` | `string` | Target scope. One of: `"status"`, `"body"`, `"headers"`, `"bodySize"`, `"totalDelayMs"`, `"dns"`, `"connect"`, `"tls"`, `"ttfb"`, `"transfer"`, `"size"`, `"redirects"`. |
| `op` | `string` | Comparison operator: `"lt"`, `"lte"`, `"eq"`, `"neq"`, `"gte"`, `"gt"`. |
| `match` | `string` | Only present on `"redirects"` scope. One of `"first"`, `"last"`, `"any"`. Selects which redirect URL is compared. |
| `outcome` | `string` | `"passed"`, `"failed"`, or `"indeterminate"`. |
| `actual` | `any` | Observed value. Type depends on scope (integer for status/timing, string for body, etc.). |
| `expected` | `any` | Expected value as written in source, after variable resolution. |
| `options` | `object \| null` | The `options {}` block from source, passed through opaquely. Extensions read this. `null` if no options block was written. |

```json
{
  "method": "expect",
  "scope": "status",
  "op": "eq",
  "outcome": "passed",
  "actual": 200,
  "expected": 200,
  "options": null
}
```

**`expect` vs `check`:** An `.expect()` assertion with `"failed"` outcome causes the
overall call (and run) to fail. A `.check()` assertion records the result but does not
affect the outcome -- it is a soft assertion.

### ConditionAssertion (`.assert()`)

Produced by `.assert()` chain methods that evaluate free-form expressions.

| Field | Type | Description |
|---|---|---|
| `method` | `string` | Always `"assert"`. |
| `kind` | `string` | `"expect"` or `"check"` -- determines hard vs soft semantics. |
| `index` | `integer` | Zero-based index of this condition within its `.assert()` call. |
| `outcome` | `string` | `"passed"`, `"failed"`, or `"indeterminate"`. |
| `expression` | `string` | The expression as written in source. |
| `actualLhs` | `any` | Resolved left-hand operand value. May be any JSON type or `null`. |
| `actualRhs` | `any` | Resolved right-hand operand value. May be any JSON type or `null`. |
| `options` | `object \| null` | The `options {}` block from source, or `null`. |

```json
{
  "method": "assert",
  "kind": "expect",
  "index": 0,
  "outcome": "failed",
  "expression": "$$countAfter eq $$countBefore + 1",
  "actualLhs": 42,
  "actualRhs": 41,
  "options": null
}
```

### Assertion outcome values

| Value | Meaning |
|---|---|
| `"passed"` | The assertion condition was met. |
| `"failed"` | The assertion condition was not met. For `method: "expect"` / `kind: "expect"`, this causes a run failure. |
| `"indeterminate"` | The assertion could not be evaluated (e.g. a referenced variable was `null`). |

---

## Skipped call records

When an earlier call fails and subsequent calls are skipped due to the failure cascade,
each skipped call still appears in the `calls` array with:

- `outcome`: `"skipped"`
- `startedAt`: `null`
- `endedAt`: `null`
- `request`: `null`
- `response`: `null`
- `assertions`: `[]` (empty array)
- `redirects`: `[]` (empty array)
- `warnings`: `[]` (empty array)
- `error`: `null`

```json
{
  "index": 2,
  "outcome": "skipped",
  "startedAt": null,
  "endedAt": null,
  "request": null,
  "response": null,
  "redirects": [],
  "assertions": [],
  "config": {},
  "warnings": [],
  "error": null
}
```

This guarantees that `calls.length` always equals the number of calls in the source
script, and `calls[n].index === n`.

---

## Config

The `config` object contains the resolved configuration for the call after defaults have
been applied. Extensions may register additional fields.

```json
{
  "timeout": { "ms": 5000, "action": "fail", "retries": 0 },
  "redirects": { "follow": true, "max": 10 },
  "security": { "rejectInvalidCerts": true }
}
```

---

## Warnings and errors

`warnings` is an array of human-readable strings for non-fatal issues encountered
during the call:

- Null variable interpolations (a `$var` resolved to `null`)
- TLS warnings (e.g. certificate close to expiry)
- Skipped `$$var` writes due to the write-once rule

`error` is a string describing a non-assertion failure, or `null`. Typical values:

- `"Connection refused"`
- `"TLS handshake failed: certificate expired"`
- `"Redirect limit exceeded (10)"`
- `"Response body too large (> 10MB)"`
