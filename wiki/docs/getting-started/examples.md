# Examples

These examples are runnable scripts from the [`examples/`](https://github.com/tracedown/lacelang/tree/main/examples) directory. Each subdirectory contains a `.lace` script and the `result.json` produced by the Python reference executor.

---

## Service status monitoring

The simplest possible probe -- send a GET request and assert on the status code.

```lace
get("https://www.google.com/")
    .expect(status: [200, 301, 302])
```

**What it does:** Sends a GET request and hard-fails if the status is not one of 200, 301, or 302. The array syntax means "any of these values passes."

**Key result fields:**

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 639,
      "calls": [
        {
          "outcome": "success",
          "response": {
            "status": 200,
            "statusText": "OK",
            "responseTimeMs": 637,
            "dnsMs": 8,
            "connectMs": 31,
            "tlsMs": 50,
            "ttfbMs": 516,
            "transferMs": 100,
            "sizeBytes": 79243
          },
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "op": "eq",
              "outcome": "passed",
              "actual": 200,
              "expected": [200, 301, 302]
            }
          ]
        }
      ]
    }
    ```

=== "Full"

    ```json
    {
      "outcome": "success",
      "startedAt": "1970-01-01T00:00:00.000Z",
      "endedAt": "1970-01-01T00:00:00.000Z",
      "elapsedMs": 639,
      "runVars": {},
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "startedAt": "1970-01-01T00:00:00.000Z",
          "endedAt": "1970-01-01T00:00:00.000Z",
          "request": {
            "url": "https://www.google.com/",
            "method": "get",
            "headers": {
              "User-Agent": "lace-probe/0.1.0 (lacelang-python)"
            },
            "bodyPath": null
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "expires": "-1",
              "cache-control": "private, max-age=0",
              "content-type": "text/html; charset=ISO-8859-1",
              "content-security-policy-report-only": "object-src 'none';base-uri 'self';script-src ...;report-uri https://csp.withgoogle.com/csp/gws/other-hp",
              "accept-ch": "Sec-CH-Prefers-Color-Scheme",
              "p3p": "CP=\"This is not a P3P policy! See g.co/p3phelp for more info.\"",
              "server": "gws",
              "x-xss-protection": "0",
              "x-frame-options": "SAMEORIGIN",
              "alt-svc": "h3=\":443\"; ma=2592000,h3-29=\":443\"; ma=2592000",
              "accept-ranges": "none",
              "vary": "Accept-Encoding",
              "transfer-encoding": "chunked"
            },
            "bodyPath": "bodies/call_0_response.html",
            "responseTimeMs": 637,
            "dnsMs": 8,
            "connectMs": 31,
            "tlsMs": 50,
            "ttfbMs": 516,
            "transferMs": 100,
            "sizeBytes": 79243,
            "dns": {
              "resolvedIps": ["203.0.113.1", "203.0.113.2"],
              "resolvedIp": "203.0.113.1"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "http/1.1",
              "certificate": {
                "subject": { "cn": "www.google.com" },
                "subjectAltNames": ["DNS:www.google.com"],
                "issuer": { "cn": "WR2" },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
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
              "expected": [200, 301, 302],
              "options": null
            }
          ],
          "config": {},
          "warnings": [],
          "error": null
        }
      ],
      "actions": {}
    }
    ```

Even for this minimal probe, the result includes a full timing breakdown and TLS/DNS metadata. See `examples/service-status/result.json` for the complete output.

---

## Time-scoped monitoring

Hard-fail if total response exceeds 3 seconds, soft-warn if individual timing phases are slow.

```lace
get("https://www.google.com/", {
  headers: { Accept: "text/html" }
})
.expect(
  status: 200,
  totalDelayMs: { value: 3000 }
)
.check(
  dns:      { value: 80 },
  connect:  { value: 150 },
  tls:      { value: 200 },
  ttfb:     { value: 500 },
  transfer: { value: 400 }
)
```

**What it does:** The `.expect()` block contains the hard requirements -- status 200, total time under 3 seconds. The `.check()` block contains soft thresholds for each timing phase. If TTFB exceeds 500ms, the check fails but execution continues and all other timing checks are still evaluated.

**Key result fields -- note the soft failure:**

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 716,
      "assertions": [
        {
          "method": "expect",
          "scope": "status",
          "outcome": "passed",
          "actual": 200,
          "expected": 200
        },
        {
          "method": "expect",
          "scope": "totalDelayMs",
          "outcome": "passed",
          "actual": 715,
          "expected": 3000
        },
        {
          "method": "check",
          "scope": "dns",
          "outcome": "passed",
          "actual": 5,
          "expected": 80
        },
        {
          "method": "check",
          "scope": "connect",
          "outcome": "passed",
          "actual": 36,
          "expected": 150
        },
        {
          "method": "check",
          "scope": "tls",
          "outcome": "passed",
          "actual": 48,
          "expected": 200
        },
        {
          "method": "check",
          "scope": "ttfb",
          "outcome": "failed",
          "actual": 533,
          "expected": 500
        },
        {
          "method": "check",
          "scope": "transfer",
          "outcome": "passed",
          "actual": 161,
          "expected": 400
        }
      ]
    }
    ```

=== "Full"

    ```json
    {
      "outcome": "success",
      "startedAt": "1970-01-01T00:00:00.000Z",
      "endedAt": "1970-01-01T00:00:00.000Z",
      "elapsedMs": 716,
      "runVars": {},
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "startedAt": "1970-01-01T00:00:00.000Z",
          "endedAt": "1970-01-01T00:00:00.000Z",
          "request": {
            "url": "https://www.google.com/",
            "method": "get",
            "headers": {
              "Accept": "text/html",
              "User-Agent": "lace-probe/0.1.0 (lacelang-python)"
            },
            "bodyPath": null
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "expires": "-1",
              "cache-control": "private, max-age=0",
              "content-type": "text/html; charset=ISO-8859-1",
              "content-security-policy-report-only": "object-src 'none';base-uri 'self';script-src ...;report-uri https://csp.withgoogle.com/csp/gws/other-hp",
              "accept-ch": "Sec-CH-Prefers-Color-Scheme",
              "p3p": "CP=\"This is not a P3P policy! See g.co/p3phelp for more info.\"",
              "server": "gws",
              "x-xss-protection": "0",
              "x-frame-options": "SAMEORIGIN",
              "alt-svc": "h3=\":443\"; ma=2592000,h3-29=\":443\"; ma=2592000",
              "accept-ranges": "none",
              "vary": "Accept-Encoding",
              "transfer-encoding": "chunked"
            },
            "bodyPath": "bodies/call_0_response.html",
            "responseTimeMs": 715,
            "dnsMs": 5,
            "connectMs": 36,
            "tlsMs": 48,
            "ttfbMs": 533,
            "transferMs": 161,
            "sizeBytes": 79239,
            "dns": {
              "resolvedIps": ["203.0.113.1", "203.0.113.2"],
              "resolvedIp": "203.0.113.1"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "http/1.1",
              "certificate": {
                "subject": { "cn": "www.google.com" },
                "subjectAltNames": ["DNS:www.google.com"],
                "issuer": { "cn": "WR2" },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
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
            },
            {
              "method": "expect",
              "scope": "totalDelayMs",
              "op": "lt",
              "outcome": "passed",
              "actual": 715,
              "expected": 3000,
              "options": null
            },
            {
              "method": "check",
              "scope": "dns",
              "op": "lt",
              "outcome": "passed",
              "actual": 5,
              "expected": 80,
              "options": null
            },
            {
              "method": "check",
              "scope": "connect",
              "op": "lt",
              "outcome": "passed",
              "actual": 36,
              "expected": 150,
              "options": null
            },
            {
              "method": "check",
              "scope": "tls",
              "op": "lt",
              "outcome": "passed",
              "actual": 48,
              "expected": 200,
              "options": null
            },
            {
              "method": "check",
              "scope": "ttfb",
              "op": "lt",
              "outcome": "failed",
              "actual": 533,
              "expected": 500,
              "options": null
            },
            {
              "method": "check",
              "scope": "transfer",
              "op": "lt",
              "outcome": "passed",
              "actual": 161,
              "expected": 400,
              "options": null
            }
          ],
          "config": {
            "headers": {
              "Accept": "text/html"
            }
          },
          "warnings": [],
          "error": null
        }
      ],
      "actions": {}
    }
    ```

The overall call outcome is still `"success"` because only `.check()` failed -- the TTFB threshold is a soft warning. The monitoring backend can decide how to handle it. See `examples/time-scoped-monitoring/result.json` for the full output.

---

## Chained requests

POST to one endpoint, capture a value with `.store()`, then use it in a subsequent request.

```lace
post("https://httpbin.org/post", {
  body: json({ email: "user@example.com", action: "login" })
})
.expect(status: 200)
.store({ "$$origin": this.body.origin })

get("https://httpbin.org/get", {
  headers: { Origin: "$$origin" }
})
.expect(status: 200)
.check(totalDelayMs: { value: 1000 })
```

**What it does:** The first call POSTs JSON data and captures `this.body.origin` (the IP address httpbin echoes back) into a run-scope variable `$$origin`. The second call uses `$$origin` as a header value.

!!! note "Run-scope vs. write-back variables"
    `$$origin` (double dollar) is a run-scope variable -- it lives in memory for the duration of this execution and appears in `runVars`. A single-dollar `$var` would signal the backend to persist the value for future runs.

**Key result fields:**

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 1181,
      "runVars": {
        "origin": "203.0.113.1"
      },
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "request": {
            "url": "https://httpbin.org/post",
            "method": "post"
          },
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "outcome": "passed",
              "actual": 200,
              "expected": 200
            }
          ]
        },
        {
          "index": 1,
          "outcome": "success",
          "request": {
            "url": "https://httpbin.org/get",
            "method": "get",
            "headers": {
              "Origin": "203.0.113.1"
            }
          },
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "outcome": "passed",
              "actual": 200,
              "expected": 200
            },
            {
              "method": "check",
              "scope": "totalDelayMs",
              "outcome": "passed",
              "actual": 507,
              "expected": 1000
            }
          ]
        }
      ]
    }
    ```

=== "Full"

    ```json
    {
      "outcome": "success",
      "startedAt": "1970-01-01T00:00:00.000Z",
      "endedAt": "1970-01-01T00:00:00.000Z",
      "elapsedMs": 1181,
      "runVars": {
        "origin": "203.0.113.1"
      },
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "startedAt": "1970-01-01T00:00:00.000Z",
          "endedAt": "1970-01-01T00:00:00.000Z",
          "request": {
            "url": "https://httpbin.org/post",
            "method": "post",
            "headers": {
              "Content-Type": "application/json",
              "User-Agent": "lace-probe/0.1.0 (lacelang-python)"
            },
            "bodyPath": "bodies/call_0_request.json"
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "content-type": "application/json",
              "content-length": "539",
              "connection": "keep-alive",
              "server": "gunicorn/19.9.0",
              "access-control-allow-origin": "*",
              "access-control-allow-credentials": "true"
            },
            "bodyPath": "bodies/call_0_response.json",
            "responseTimeMs": 659,
            "dnsMs": 16,
            "connectMs": 114,
            "tlsMs": 240,
            "ttfbMs": 637,
            "transferMs": 0,
            "sizeBytes": 539,
            "dns": {
              "resolvedIps": ["203.0.113.1", "203.0.113.2"],
              "resolvedIp": "203.0.113.1"
            },
            "tls": {
              "protocol": "TLSv1.2",
              "cipher": "ECDHE-RSA-AES128-GCM-SHA256",
              "alpn": "http/1.1",
              "certificate": {
                "subject": { "cn": "httpbin.org" },
                "subjectAltNames": ["DNS:httpbin.org", "DNS:*.httpbin.org"],
                "issuer": { "cn": "Amazon RSA 2048 M03" },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
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
            "body": {
              "type": "json",
              "value": "{email: \"user@example.com\", action: \"login\"}"
            }
          },
          "warnings": [],
          "error": null
        },
        {
          "index": 1,
          "outcome": "success",
          "startedAt": "1970-01-01T00:00:00.000Z",
          "endedAt": "1970-01-01T00:00:00.000Z",
          "request": {
            "url": "https://httpbin.org/get",
            "method": "get",
            "headers": {
              "Origin": "203.0.113.1",
              "User-Agent": "lace-probe/0.1.0 (lacelang-python)"
            },
            "bodyPath": null
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "content-type": "application/json",
              "content-length": "324",
              "connection": "keep-alive",
              "server": "gunicorn/19.9.0",
              "access-control-allow-origin": "203.0.113.1",
              "access-control-allow-credentials": "true"
            },
            "bodyPath": "bodies/call_1_response.json",
            "responseTimeMs": 507,
            "dnsMs": 0,
            "connectMs": 117,
            "tlsMs": 245,
            "ttfbMs": 491,
            "transferMs": 0,
            "sizeBytes": 324,
            "dns": {
              "resolvedIps": ["203.0.113.1", "203.0.113.2"],
              "resolvedIp": "203.0.113.1"
            },
            "tls": {
              "protocol": "TLSv1.2",
              "cipher": "ECDHE-RSA-AES128-GCM-SHA256",
              "alpn": "http/1.1",
              "certificate": {
                "subject": { "cn": "httpbin.org" },
                "subjectAltNames": ["DNS:httpbin.org", "DNS:*.httpbin.org"],
                "issuer": { "cn": "Amazon RSA 2048 M03" },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
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
            },
            {
              "method": "check",
              "scope": "totalDelayMs",
              "op": "lt",
              "outcome": "passed",
              "actual": 507,
              "expected": 1000,
              "options": null
            }
          ],
          "config": {
            "headers": {
              "Origin": "$$origin"
            }
          },
          "warnings": [],
          "error": null
        }
      ],
      "actions": {}
    }
    ```

If the first call had failed its `.expect()`, the second call would be skipped entirely -- that is the hard-fail cascade. See `examples/chained-requests/result.json` for the full output.

---

## Expect vs. check -- hard and soft failures

A direct demonstration of the two assertion modes.

```lace
get("https://www.google.com/")
.expect(status: 200)
.check(
  totalDelayMs: { value: 1000 },
  ttfb: { value: 200 }
)
```

**What it does:** The status check is a hard requirement. The two timing checks are soft -- both are always evaluated, even if one fails.

**Key result fields:**

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 700,
      "assertions": [
        {
          "method": "expect",
          "scope": "status",
          "outcome": "passed",
          "actual": 200,
          "expected": 200
        },
        {
          "method": "check",
          "scope": "totalDelayMs",
          "outcome": "passed",
          "actual": 697,
          "expected": 1000
        },
        {
          "method": "check",
          "scope": "ttfb",
          "outcome": "failed",
          "actual": 535,
          "expected": 200
        }
      ]
    }
    ```

=== "Full"

    ```json
    {
      "outcome": "success",
      "startedAt": "1970-01-01T00:00:00.000Z",
      "endedAt": "1970-01-01T00:00:00.000Z",
      "elapsedMs": 700,
      "runVars": {},
      "calls": [
        {
          "index": 0,
          "outcome": "success",
          "startedAt": "1970-01-01T00:00:00.000Z",
          "endedAt": "1970-01-01T00:00:00.000Z",
          "request": {
            "url": "https://www.google.com/",
            "method": "get",
            "headers": {
              "User-Agent": "lace-probe/0.1.0 (lacelang-python)"
            },
            "bodyPath": null
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "expires": "-1",
              "cache-control": "private, max-age=0",
              "content-type": "text/html; charset=ISO-8859-1",
              "content-security-policy-report-only": "object-src 'none';base-uri 'self';script-src ...;report-uri https://csp.withgoogle.com/csp/gws/other-hp",
              "accept-ch": "Sec-CH-Prefers-Color-Scheme",
              "p3p": "CP=\"This is not a P3P policy! See g.co/p3phelp for more info.\"",
              "server": "gws",
              "x-xss-protection": "0",
              "x-frame-options": "SAMEORIGIN",
              "alt-svc": "h3=\":443\"; ma=2592000,h3-29=\":443\"; ma=2592000",
              "accept-ranges": "none",
              "vary": "Accept-Encoding",
              "transfer-encoding": "chunked"
            },
            "bodyPath": "bodies/call_0_response.html",
            "responseTimeMs": 697,
            "dnsMs": 5,
            "connectMs": 32,
            "tlsMs": 46,
            "ttfbMs": 535,
            "transferMs": 143,
            "sizeBytes": 79228,
            "dns": {
              "resolvedIps": ["203.0.113.1", "203.0.113.2"],
              "resolvedIp": "203.0.113.1"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "http/1.1",
              "certificate": {
                "subject": { "cn": "www.google.com" },
                "subjectAltNames": ["DNS:www.google.com"],
                "issuer": { "cn": "WR2" },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
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
            },
            {
              "method": "check",
              "scope": "totalDelayMs",
              "op": "lt",
              "outcome": "passed",
              "actual": 697,
              "expected": 1000,
              "options": null
            },
            {
              "method": "check",
              "scope": "ttfb",
              "op": "lt",
              "outcome": "failed",
              "actual": 535,
              "expected": 200,
              "options": null
            }
          ],
          "config": {},
          "warnings": [],
          "error": null
        }
      ],
      "actions": {}
    }
    ```

The TTFB exceeded the 200ms soft threshold (actual: 535ms), but the overall call outcome is `"success"` and the total delay check still ran and passed. A monitoring backend can aggregate these soft failures over time to detect degradation trends without triggering immediate alerts.

See `examples/expect-vs-check/result.json` for the full output.

---

## Notification templates

Using the `laceNotifications` extension to send different notification templates based on which status code was returned.

```lace
get("$BASE_URL/api/orders")
.expect(
  status: {
    value: 200,
    options: {
      notification: op_map({
        "404": template("not_found_alert"),
        "500": template("server_error_alert"),
        "502": template("gateway_error_alert"),
        "503": template("service_unavailable_alert"),
        "default": template("unexpected_status_alert")
      })
    }
  }
)
```

**What it does:** Expects status 200. The `options.notification` field (registered by the `laceNotifications` extension) uses `op_map` to map specific failure status codes to named notification templates. When the endpoint returns 404, the extension resolves the `"404"` key and emits the corresponding `not_found_alert` template.

!!! info "Prerequisites"
    This example requires the `laceNotifications` extension to be active. The `lace.config` in the example directory enables it:

    ```toml
    [executor]
    extensions = ["laceNotifications"]
    ```

    Variables are injected via `vars.json`:

    ```json
    { "BASE_URL": "https://httpbin.org" }
    ```

**Key result fields -- the assertion and the emitted notification:**

=== "Compact"

    ```json
    {
      "outcome": "failure",
      "elapsedMs": 551,
      "assertions": [
        {
          "method": "expect",
          "scope": "status",
          "op": "eq",
          "outcome": "failed",
          "actual": 404,
          "expected": 200,
          "options": {
            "notification": {
              "tag": "op_map",
              "ops": {
                "404": { "tag": "template", "name": "not_found_alert" },
                "500": { "tag": "template", "name": "server_error_alert" },
                "...": "..."
              }
            }
          }
        }
      ],
      "actions": {
        "notifications": [
          {
            "callIndex": 0,
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
      "outcome": "failure",
      "startedAt": "1970-01-01T00:00:00.000Z",
      "endedAt": "1970-01-01T00:00:00.000Z",
      "elapsedMs": 551,
      "runVars": {},
      "calls": [
        {
          "index": 0,
          "outcome": "failure",
          "startedAt": "1970-01-01T00:00:00.000Z",
          "endedAt": "1970-01-01T00:00:00.000Z",
          "request": {
            "url": "https://httpbin.org/api/orders",
            "method": "get",
            "headers": {
              "User-Agent": "lace-probe/0.1.0 (lacelang-python)"
            },
            "bodyPath": null
          },
          "response": {
            "status": 404,
            "statusText": "NOT FOUND",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "content-type": "text/html",
              "content-length": "233",
              "connection": "keep-alive",
              "server": "gunicorn/19.9.0",
              "access-control-allow-origin": "*",
              "access-control-allow-credentials": "true"
            },
            "bodyPath": "bodies/call_0_response.html",
            "responseTimeMs": 550,
            "dnsMs": 5,
            "connectMs": 137,
            "tlsMs": 247,
            "ttfbMs": 529,
            "transferMs": 1,
            "sizeBytes": 233,
            "dns": {
              "resolvedIps": ["203.0.113.1", "203.0.113.2"],
              "resolvedIp": "203.0.113.1"
            },
            "tls": {
              "protocol": "TLSv1.2",
              "cipher": "ECDHE-RSA-AES128-GCM-SHA256",
              "alpn": "http/1.1",
              "certificate": {
                "subject": { "cn": "httpbin.org" },
                "subjectAltNames": ["DNS:httpbin.org", "DNS:*.httpbin.org"],
                "issuer": { "cn": "Amazon RSA 2048 M03" },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            }
          },
          "redirects": [],
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "op": "eq",
              "outcome": "failed",
              "actual": 404,
              "expected": 200,
              "options": {
                "notification": {
                  "tag": "op_map",
                  "ops": {
                    "404": {
                      "tag": "template",
                      "name": "not_found_alert"
                    },
                    "500": {
                      "tag": "template",
                      "name": "server_error_alert"
                    },
                    "502": {
                      "tag": "template",
                      "name": "gateway_error_alert"
                    },
                    "503": {
                      "tag": "template",
                      "name": "service_unavailable_alert"
                    },
                    "default": {
                      "tag": "template",
                      "name": "unexpected_status_alert"
                    }
                  }
                }
              }
            }
          ],
          "config": {},
          "warnings": [],
          "error": null
        }
      ],
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

The assertion's `options` preserves the original `op_map` configuration, but the `actions.notifications` array contains the resolved template -- `not_found_alert`. The monitoring backend uses this to dispatch the correct notification without needing to re-evaluate the mapping logic.

See `examples/notification-templates/result.json` for the full output.
