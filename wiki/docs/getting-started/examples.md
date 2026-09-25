# Examples

These examples are runnable scripts from the [`examples/`](https://github.com/tracedown/lacelang/tree/main/examples) directory. Each subdirectory contains a `.lace` script and the `result.json` produced by the Python reference executor.

The results were produced with the default configuration -- body saving is off, so every `bodyPath` is `null` with `bodyNotCapturedReason: "notRequested"`, and each call's `config` shows the resolved defaults (timeout, redirects, security). They were then anonymised: resolved IPs become `203.0.113.x`, and timestamps and certificate dates become the Unix epoch. The **Compact** tabs show the key fields; the **Full** tabs show the complete `result.json`.

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
      "elapsedMs": 464,
      "calls": [
        {
          "outcome": "success",
          "response": {
            "status": 200,
            "statusText": "OK",
            "responseTimeMs": 462,
            "dnsMs": 5,
            "connectMs": 19,
            "tlsMs": 68,
            "ttfbMs": 364,
            "transferMs": 58,
            "sizeBytes": 84704
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
      "elapsedMs": 464,
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
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "content-type": "text/html; charset=ISO-8859-1",
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "cache-control": "private, max-age=0",
              "content-security-policy-report-only": "object-src 'none';base-uri 'self';script-src 'nonce-_ICPX2zqmTa_NHGKLZ8IbQ' 'strict-dynamic' 'report-sample' 'unsafe-eval' 'unsafe-inline' https: http:;report-uri https://csp.withgoogle.com/csp/gws/other-hp",
              "accept-ch": "Sec-CH-Prefers-Color-Scheme",
              "p3p": "CP=\"This is not a P3P policy! See g.co/p3phelp for more info.\"",
              "server": "gws",
              "x-xss-protection": "0",
              "x-frame-options": "SAMEORIGIN",
              "accept-ranges": "none",
              "vary": "Accept-Encoding",
              "transfer-encoding": "chunked"
            },
            "bodyPath": null,
            "responseTimeMs": 462,
            "dnsMs": 5,
            "connectMs": 19,
            "tlsMs": 68,
            "ttfbMs": 364,
            "transferMs": 58,
            "sizeBytes": 84704,
            "dns": {
              "resolvedIps": [
                "203.0.113.11",
                "203.0.113.12",
                "203.0.113.13",
                "203.0.113.16",
                "203.0.113.17",
                "203.0.113.10",
                "203.0.113.14",
                "203.0.113.15",
                "2001:4860:4826:7700::",
                "2001:4860:4828:7700::",
                "2001:4860:482a:7700::",
                "2001:4860:482d:7700::",
                "2001:4860:4829:7700::",
                "2001:4860:482b:7700::",
                "2001:4860:482c:7700::",
                "2001:4860:4827:7700::"
              ],
              "resolvedIp": "203.0.113.11"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "http/1.1",
              "certificate": {
                "subject": {
                  "cn": "www.google.com"
                },
                "subjectAltNames": [
                  "DNS:www.google.com"
                ],
                "issuer": {
                  "cn": "WE2"
                },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            },
            "bodyNotCapturedReason": "notRequested"
          },
          "redirects": [],
          "assertions": [
            {
              "method": "expect",
              "scope": "status",
              "op": "eq",
              "outcome": "passed",
              "actual": 200,
              "expected": [
                200,
                301,
                302
              ],
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

Even for this minimal probe, the result includes a full timing breakdown and TLS/DNS metadata.

---

## Time-scoped monitoring

Hard-fail if the total response time reaches 3 seconds, soft-fail if individual timing phases are slow.

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

**Key result fields:**

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 423,
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
          "actual": 421,
          "expected": 3000
        },
        {
          "method": "check",
          "scope": "dns",
          "outcome": "passed",
          "actual": 4,
          "expected": 80
        },
        {
          "method": "check",
          "scope": "connect",
          "outcome": "passed",
          "actual": 18,
          "expected": 150
        },
        {
          "method": "check",
          "scope": "tls",
          "outcome": "passed",
          "actual": 57,
          "expected": 200
        },
        {
          "method": "check",
          "scope": "ttfb",
          "outcome": "passed",
          "actual": 326,
          "expected": 500
        },
        {
          "method": "check",
          "scope": "transfer",
          "outcome": "passed",
          "actual": 57,
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
      "elapsedMs": 423,
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
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "content-type": "text/html; charset=ISO-8859-1",
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "cache-control": "private, max-age=0",
              "content-security-policy-report-only": "object-src 'none';base-uri 'self';script-src 'nonce-vd4jr41c-NVHgkYYwoMH1g' 'strict-dynamic' 'report-sample' 'unsafe-eval' 'unsafe-inline' https: http:;report-uri https://csp.withgoogle.com/csp/gws/other-hp",
              "accept-ch": "Sec-CH-Prefers-Color-Scheme",
              "p3p": "CP=\"This is not a P3P policy! See g.co/p3phelp for more info.\"",
              "server": "gws",
              "x-xss-protection": "0",
              "x-frame-options": "SAMEORIGIN",
              "accept-ranges": "none",
              "vary": "Accept-Encoding",
              "transfer-encoding": "chunked"
            },
            "bodyPath": null,
            "responseTimeMs": 421,
            "dnsMs": 4,
            "connectMs": 18,
            "tlsMs": 57,
            "ttfbMs": 326,
            "transferMs": 57,
            "sizeBytes": 84762,
            "dns": {
              "resolvedIps": [
                "203.0.113.11",
                "203.0.113.10",
                "203.0.113.17",
                "203.0.113.15",
                "203.0.113.12",
                "203.0.113.16",
                "203.0.113.14",
                "203.0.113.13",
                "2001:4860:4829:7700::",
                "2001:4860:4828:7700::",
                "2001:4860:482c:7700::",
                "2001:4860:4827:7700::",
                "2001:4860:4826:7700::",
                "2001:4860:482a:7700::",
                "2001:4860:482d:7700::",
                "2001:4860:482b:7700::"
              ],
              "resolvedIp": "203.0.113.11"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "http/1.1",
              "certificate": {
                "subject": {
                  "cn": "www.google.com"
                },
                "subjectAltNames": [
                  "DNS:www.google.com"
                ],
                "issuer": {
                  "cn": "WE2"
                },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            },
            "bodyNotCapturedReason": "notRequested"
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
              "actual": 421,
              "expected": 3000,
              "options": null
            },
            {
              "method": "check",
              "scope": "dns",
              "op": "lt",
              "outcome": "passed",
              "actual": 4,
              "expected": 80,
              "options": null
            },
            {
              "method": "check",
              "scope": "connect",
              "op": "lt",
              "outcome": "passed",
              "actual": 18,
              "expected": 150,
              "options": null
            },
            {
              "method": "check",
              "scope": "tls",
              "op": "lt",
              "outcome": "passed",
              "actual": 57,
              "expected": 200,
              "options": null
            },
            {
              "method": "check",
              "scope": "ttfb",
              "op": "lt",
              "outcome": "passed",
              "actual": 326,
              "expected": 500,
              "options": null
            },
            {
              "method": "check",
              "scope": "transfer",
              "op": "lt",
              "outcome": "passed",
              "actual": 57,
              "expected": 400,
              "options": null
            }
          ],
          "config": {
            "headers": {
              "Accept": "text/html"
            },
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

In this run every threshold passed. Had TTFB reached 500ms, the `ttfb` check would be recorded as `"failed"`, the remaining timing checks would still be evaluated, and the call outcome would stay `"success"` -- a `.check()` failure is a soft failure. The monitoring backend can decide how to handle it.

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
      "elapsedMs": 1253,
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
              "actual": 539,
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
      "elapsedMs": 1253,
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
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "content-type": "application/json",
              "content-length": "540",
              "connection": "keep-alive",
              "server": "gunicorn/19.9.0",
              "access-control-allow-origin": "*",
              "access-control-allow-credentials": "true"
            },
            "bodyPath": null,
            "responseTimeMs": 710,
            "dnsMs": 23,
            "connectMs": 147,
            "tlsMs": 244,
            "ttfbMs": 666,
            "transferMs": 0,
            "sizeBytes": 540,
            "dns": {
              "resolvedIps": [
                "203.0.113.2",
                "203.0.113.3",
                "203.0.113.4",
                "203.0.113.5",
                "203.0.113.6",
                "203.0.113.7",
                "203.0.113.8",
                "203.0.113.9"
              ],
              "resolvedIp": "203.0.113.2"
            },
            "tls": {
              "protocol": "TLSv1.2",
              "cipher": "ECDHE-RSA-AES128-GCM-SHA256",
              "alpn": "http/1.1",
              "certificate": {
                "subject": {
                  "cn": "httpbin.org"
                },
                "subjectAltNames": [
                  "DNS:httpbin.org",
                  "DNS:*.httpbin.org"
                ],
                "issuer": {
                  "cn": "Amazon RSA 2048 M01"
                },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            },
            "bodyNotCapturedReason": "notRequested"
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
              "value": {
                "email": "user@example.com",
                "action": "login"
              }
            },
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
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "content-type": "application/json",
              "content-length": "326",
              "connection": "keep-alive",
              "server": "gunicorn/19.9.0",
              "access-control-allow-origin": "203.0.113.1",
              "access-control-allow-credentials": "true"
            },
            "bodyPath": null,
            "responseTimeMs": 539,
            "dnsMs": 22,
            "connectMs": 109,
            "tlsMs": 221,
            "ttfbMs": 482,
            "transferMs": 0,
            "sizeBytes": 326,
            "dns": {
              "resolvedIps": [
                "203.0.113.3",
                "203.0.113.9",
                "203.0.113.5",
                "203.0.113.8",
                "203.0.113.6",
                "203.0.113.7",
                "203.0.113.2",
                "203.0.113.4"
              ],
              "resolvedIp": "203.0.113.3"
            },
            "tls": {
              "protocol": "TLSv1.2",
              "cipher": "ECDHE-RSA-AES128-GCM-SHA256",
              "alpn": "http/1.1",
              "certificate": {
                "subject": {
                  "cn": "httpbin.org"
                },
                "subjectAltNames": [
                  "DNS:httpbin.org",
                  "DNS:*.httpbin.org"
                ],
                "issuer": {
                  "cn": "Amazon RSA 2048 M01"
                },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            },
            "bodyNotCapturedReason": "notRequested"
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
              "actual": 539,
              "expected": 1000,
              "options": null
            }
          ],
          "config": {
            "headers": {
              "Origin": "203.0.113.1"
            },
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

If the first call had failed its `.expect()`, its `.store()` would not run and the second call would be skipped -- it still appears in `calls` with `outcome: "skipped"`. That is the hard-fail cascade.

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
      "elapsedMs": 570,
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
          "actual": 568,
          "expected": 1000
        },
        {
          "method": "check",
          "scope": "ttfb",
          "outcome": "failed",
          "actual": 448,
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
      "elapsedMs": 570,
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
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
          },
          "response": {
            "status": 200,
            "statusText": "OK",
            "headers": {
              "content-type": "text/html; charset=ISO-8859-1",
              "date": "Thu, 01 Jan 1970 00:00:00 GMT",
              "cache-control": "private, max-age=0",
              "content-security-policy-report-only": "object-src 'none';base-uri 'self';script-src 'nonce-KhIbZLaPVpEgY0dXpKJ-8w' 'strict-dynamic' 'report-sample' 'unsafe-eval' 'unsafe-inline' https: http:;report-uri https://csp.withgoogle.com/csp/gws/other-hp",
              "accept-ch": "Sec-CH-Prefers-Color-Scheme",
              "p3p": "CP=\"This is not a P3P policy! See g.co/p3phelp for more info.\"",
              "server": "gws",
              "x-xss-protection": "0",
              "x-frame-options": "SAMEORIGIN",
              "accept-ranges": "none",
              "vary": "Accept-Encoding",
              "transfer-encoding": "chunked"
            },
            "bodyPath": null,
            "responseTimeMs": 568,
            "dnsMs": 119,
            "connectMs": 18,
            "tlsMs": 59,
            "ttfbMs": 448,
            "transferMs": 66,
            "sizeBytes": 84696,
            "dns": {
              "resolvedIps": [
                "203.0.113.10",
                "203.0.113.11",
                "203.0.113.12",
                "203.0.113.13",
                "203.0.113.14",
                "203.0.113.15",
                "203.0.113.16",
                "203.0.113.17",
                "2001:4860:482a:7700::",
                "2001:4860:482d:7700::",
                "2001:4860:4829:7700::",
                "2001:4860:4828:7700::",
                "2001:4860:4826:7700::",
                "2001:4860:4827:7700::",
                "2001:4860:482c:7700::",
                "2001:4860:482b:7700::"
              ],
              "resolvedIp": "203.0.113.10"
            },
            "tls": {
              "protocol": "TLSv1.3",
              "cipher": "TLS_AES_256_GCM_SHA384",
              "alpn": "http/1.1",
              "certificate": {
                "subject": {
                  "cn": "www.google.com"
                },
                "subjectAltNames": [
                  "DNS:www.google.com"
                ],
                "issuer": {
                  "cn": "WE2"
                },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            },
            "bodyNotCapturedReason": "notRequested"
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
              "actual": 568,
              "expected": 1000,
              "options": null
            },
            {
              "method": "check",
              "scope": "ttfb",
              "op": "lt",
              "outcome": "failed",
              "actual": 448,
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

The TTFB exceeded the 200ms soft threshold (actual: 448ms), but the overall call outcome is `"success"` and the total delay check still ran and passed. A monitoring backend can aggregate these soft failures over time to detect degradation trends without triggering immediate alerts.

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

    Variables are injected from `vars.json`, passed with `--vars vars.json`:

    ```json
    { "BASE_URL": "https://httpbin.org" }
    ```

**Key result fields -- the assertion and the emitted notification:**

=== "Compact"

    ```json
    {
      "outcome": "failure",
      "elapsedMs": 602,
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
            "notification": { "tag": "template", "name": "not_found_alert" }
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
      "elapsedMs": 602,
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
              "User-Agent": "lace-probe/0.2.0 (lacelang-python)"
            }
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
            "bodyPath": null,
            "responseTimeMs": 600,
            "dnsMs": 25,
            "connectMs": 184,
            "tlsMs": 215,
            "ttfbMs": 556,
            "transferMs": 0,
            "sizeBytes": 233,
            "dns": {
              "resolvedIps": [
                "203.0.113.2",
                "203.0.113.3",
                "203.0.113.9",
                "203.0.113.5",
                "203.0.113.7",
                "203.0.113.8",
                "203.0.113.4",
                "203.0.113.6"
              ],
              "resolvedIp": "203.0.113.2"
            },
            "tls": {
              "protocol": "TLSv1.2",
              "cipher": "ECDHE-RSA-AES128-GCM-SHA256",
              "alpn": "http/1.1",
              "certificate": {
                "subject": {
                  "cn": "httpbin.org"
                },
                "subjectAltNames": [
                  "DNS:httpbin.org",
                  "DNS:*.httpbin.org"
                ],
                "issuer": {
                  "cn": "Amazon RSA 2048 M01"
                },
                "notBefore": "1970-01-01T00:00:00.000Z",
                "notAfter": "1970-01-01T00:00:00.000Z"
              }
            },
            "bodyNotCapturedReason": "notRequested"
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
