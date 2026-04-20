# Why Lace?

API monitoring sits in a gap between two worlds. On one side, testing tools like Postman, Hurl, and REST Client are designed for developers writing one-off checks during development. On the other, observability platforms like Datadog Synthetics and Grafana k6 are monolithic services that own the entire pipeline from probe definition to alerting. Neither gives you a portable, backend-agnostic probe language that you own and that runs identically everywhere.

Lace fills this gap. It is a purpose-built scripting microlanguage for defining API monitoring probes -- not a testing framework, not an observability platform. A language.

## What makes Lace different

### Structured results, not pass/fail verdicts

A Lace probe does not print "PASS" or "FAIL" to a terminal. It returns a structured JSON result containing per-call timing breakdowns, assertion outcomes with actual and expected values, captured variables, and extension-emitted actions. The result is designed for machine consumption by a monitoring backend.

### Stateful across runs

Every Lace execution can receive the previous run's result via `prev`. Scripts and extensions can read `prev.outcome`, `prev.calls[0].response.status`, and `prev.runVars.*` to implement change detection, repeat suppression, and rolling baselines. This is a first-class language feature.

Testing tools typically keep each run isolated -- which is correct for tests. But monitoring probes are inherently stateful: "is this the same failure as last time?" is a question that only matters in production.

### Hard fails and soft fails

`.expect()` is a hard fail -- if it fails, subsequent calls are skipped. `.check()` is a soft fail -- the failure is recorded but execution continues. A probe that checks five timing metrics should report all five, not stop at the first failure.

Both methods evaluate all scopes completely before cascading, so the result always shows the full picture of what failed.

### Timing as a first-class citizen

Every call in Lace's result includes a full timing breakdown: DNS resolution, TCP connect, TLS handshake, time to first byte, and body transfer. Each phase is available as an assertion scope:

```lace
.expect(dns: 50, tls: 200)
.check(ttfb: { value: 500 }, connect: { value: 150 })
```

For monitoring, knowing *why* a request was slow is as important as knowing *that* it was slow.

### Declarative extension system

The `.laceext` format lets you add schema fields, result actions, and hook-based rules without modifying the core executor. Extensions are TOML files with a formal rule body language -- the same file runs identically on every executor implementation.

Built-in extensions include `laceNotifications` (assertion failure alerting with custom templates and repeat suppression) and `laceBaseline` (rolling average monitoring with spike detection).

### Backend-agnostic by design

The executor is deliberately side-effect-free. It does not send emails, post to Slack, or write to databases. It returns a result. The monitoring platform that runs the probe decides what to do. This makes Lace usable inside any backend -- from a cron job piping JSON to `jq`, to a distributed monitoring fleet with custom alerting rules.

## Lace vs. Hurl

Hurl occupies the same syntactic space (text files describing HTTP calls) but serves a different purpose. The differences are architectural.

| Concern | Lace | Hurl |
|---|---|---|
| **Primary purpose** | Production API monitoring probes | HTTP request testing |
| **Output format** | Structured JSON with timing, assertions, actions | Pass/fail verdict, optional JSON report |
| **Hard vs. soft failures** | `.expect()` (hard) and `.check()` (soft) -- both evaluate all scopes | `[Asserts]` is all-or-nothing within a request entry |
| **Cross-run state** | `prev` gives access to the previous run's result | Each run is isolated |
| **Timing breakdown** | DNS, connect, TLS, TTFB, transfer -- each assertable | Duration only (single value) |
| **Extension system** | Declarative `.laceext` files with hook-based rules | No extension system |
| **Custom assertions** | `.assert()` with arbitrary expressions | Fixed assertion types |
| **Variable write-back** | `.store()` emits write-back variables for backend persistence | No write-back concept |
| **Implementations** | Multiple conformant executors (Python, JS, Kotlin) | Single binary |
| **Distribution** | Validator and executor as separate packages | Single package |

### When to use Hurl instead

Hurl is the better choice when:

- You need quick HTTP smoke tests in CI
- You want a single self-contained binary with zero configuration
- Your assertions are pass/fail with no need for structured result analysis
- You do not need cross-run state, extension hooks, or timing decomposition
- You are testing, not monitoring

Lace is not trying to replace Hurl for development-time testing. It exists because production monitoring probes have requirements that testing tools were not designed for.
