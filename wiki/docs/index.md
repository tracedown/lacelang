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
to spec version 0.9.1: [Python](getting-started/executors/python-executor.md)
(canonical), [TypeScript](getting-started/executors/ts-executor.md), and
[Kotlin/JVM](getting-started/executors/kt-executor.md).
See [Executors](getting-started/executors/index.md) for how this works.

## Your first probe

```lace
get("https://www.google.com/")
    .expect(status: [200, 301, 302])
```

Run it and you get:

=== "Compact"

    ```json
    {
      "outcome": "success",
      "elapsedMs": 639,
      "calls": [
        {
          "index": 0,
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
              "server": "gws",
              "x-frame-options": "SAMEORIGIN",
              "alt-svc": "h3=\":443\"; ma=2592000,h3-29=\":443\"; ma=2592000",
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

Every call includes a full timing breakdown (DNS, connect, TLS, TTFB, transfer), and every assertion records the actual and expected values -- not just pass/fail.

## Features

- **Hard and soft assertions** -- `.expect()` stops execution on failure; `.check()` records the failure and continues. Both evaluate all scopes before cascading.
- **Timing decomposition** -- assert on DNS, connect, TLS handshake, time to first byte, and transfer time individually.
- **Request chaining** -- capture values with `.store()` and use them in subsequent requests. Run-scope variables (`$$var`) flow forward; write-back variables (`$var`) are emitted in the result for the backend to persist.
- **Previous results** -- access the last run's data via `prev` for change detection, repeat suppression, and rolling baselines.
- **Extensions** -- declarative `.laceext` files add schema fields, result actions, and hook-based rules without modifying the core executor. Ships with `laceNotifications` and `laceBaseline`.
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
