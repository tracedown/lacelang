<p align="center">
  <img src="assets/lace-logo-256.png" alt="Lace logo" width="160" height="160">
</p>

# Lace(lang)

<a class="github-button" href="https://github.com/tracedown/lacelang" data-size="large" data-show-count="true" aria-label="Star tracedown/lacelang on GitHub">Star</a>
<a class="github-button" href="https://github.com/tracedown/lacelang/fork" data-size="large" data-show-count="true" aria-label="Fork tracedown/lacelang on GitHub">Fork</a>
<a class="github-button" href="https://github.com/tracedown/lacelang" data-size="large" aria-label="View tracedown/lacelang on GitHub">Repository</a>

Lace is a domain-specific language (DSL) for defining HTTP monitoring
probes. It is not an executable program — it is a **specification
language** that describes what to probe and what to assert. A `.lace`
file is run by an **executor**, an independent program that interprets
the script, makes the HTTP calls, evaluates assertions, and returns a
structured JSON result.

Lace defines the language. Executors implement it. Anyone can build an
executor in any programming language — the [conformance test suite](getting-started/executors/index.md)
ensures they all behave identically. Three reference implementations conform
to spec version 0.9.7<!-- sv -->: [Python](getting-started/executors/python-executor.md)
(canonical), [TypeScript](getting-started/executors/ts-executor.md), and
[Kotlin/JVM](getting-started/executors/kt-executor.md).
See [Executors](getting-started/executors/index.md) for how this works.

## Your first probe

```lace
get("https://www.google.com/")
    .expect(status: [200, 301, 302])
```

Run it and you get (default configuration, so no response body is saved; IPs and timestamps anonymised):

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 464,
      "calls": [
        {
          "index": 0,
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

Every call includes a full timing breakdown (DNS, connect, TLS, TTFB, transfer), and every assertion records the actual and expected values -- not just pass/fail.

## Features

- **Hard and soft assertions** -- `.expect()` stops execution on failure; `.check()` records the failure and continues. Both evaluate all scopes before cascading.
- **Timing decomposition** -- assert on DNS, connect, TLS handshake, time to first byte, and transfer time individually.
- **Request chaining** -- capture values with `.store()` and use them in subsequent requests. Run-scope variables (`$$var`) flow forward; write-back variables (`$var`) are emitted in the result for the backend to persist.
- **Previous results** -- access the last run's data via `prev` for change detection, repeat suppression, and rolling baselines.
- **Extensions** -- declarative `.laceext` files add schema fields, result actions, and hook-based rules without modifying the core executor. Ships with `laceNotifications`, `laceEmitRecovery`, and `laceBaseline`.
- **Backend-agnostic** -- the executor is side-effect-free. The result is a standardized JSON structure any platform can consume.
- **Portable** -- the same `.lace` file runs identically on every conformant executor (Python, JavaScript, Kotlin).

## Documentation

<div class="grid cards" markdown>

-   :material-rocket-launch: **[Getting Started](getting-started/index.md)**

    ---

    Install the executor, run your first script, learn the CLI.

-   :material-book-open-variant: **[Language](language/index.md)**

    ---

    HTTP calls, assertions, variables, chaining, failure semantics.

-   :material-puzzle: **[Extensions](extensions/index.md)**

    ---

    The `.laceext` format, hook points, built-in extensions.

-   :material-file-document: **[Reference](reference/grammar.md)**

    ---

    Grammar, result schema, error codes, scope reference.

</div>
