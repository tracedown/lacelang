# Why Lace Exists

## The Problem

Modern API monitoring sits in a gap between two worlds. On one side, testing tools (Postman, Hurl, REST Client) are designed for developers writing one-off checks during development. On the other, full observability platforms (Datadog Synthetics, Grafana k6) are monolithic services that own the entire pipeline from probe definition to alerting. Neither gives you a portable, backend-agnostic probe language that you own and that runs identically everywhere.

Lace fills this gap: a purpose-built scripting microlanguage for defining API monitoring probes. Not a testing framework. Not an observability platform. A language.

## What Lace Is

A `.lace` file is a declarative probe script. It makes HTTP calls, asserts on responses, captures values, and returns a structured result. The executor has no opinion about what happens with that result — it doesn't send alerts, write dashboards, or decide retry policy. It returns JSON and exits.

```lace
get("$BASE_URL/health")
.expect(status: 200)
.check(totalDelayMs: 500)
```

This separation is the core design decision. The language defines _what to check_. The backend decides _what to do about it_.

## Why Not Hurl

Hurl is excellent at what it does: sequential HTTP request/response testing with file-based assertions. It occupies the same syntactic space (text files describing HTTP calls) but serves a fundamentally different purpose. The differences are architectural, not cosmetic.

### Hurl is a test runner. Lace is a probe language.

Hurl produces a pass/fail verdict. Lace produces a structured result object containing per-call timing breakdowns, assertion outcomes with actual/expected values, captured variables, and extension-emitted actions. The result is designed for machine consumption by a monitoring backend, not for a human reading terminal output.

### Lace is stateful across runs.

Every Lace execution can receive the previous run's result via `prev`. Extensions and scripts can read `prev.outcome`, `prev.calls[0].response.status`, `prev.runVars.*` to implement change detection, repeat suppression, and rolling baselines. This is a first-class language feature, not a workaround.

Hurl has no concept of previous results. Each run is isolated. This is correct for testing — tests should be independent. But monitoring probes are inherently stateful: "is this the same failure as last time?" is a question that only matters in production.

### Hard fails vs soft fails.

Lace distinguishes `.expect()` (hard fail — stop execution) from `.check()` (soft fail — record and continue). A probe that checks five metrics should report all five, not stop at the first failure. Hurl's `[Asserts]` section is all-or-nothing within a request entry.

Both methods evaluate all scopes completely before cascading, so the result always shows the full picture of what failed. This is essential for a monitoring backend that needs to aggregate failures across dimensions.

### The extension system.

Lace has a declarative extension system where `.laceext` files (TOML + a rule body DSL) add schema fields, result actions, and hook-based rules. Extensions are cross-language: the same `.laceext` file runs identically on the Python, JavaScript, and Kotlin executors. The rule body language has a formal grammar and deterministic execution semantics.

This enables capabilities like `laceNotifications` (assertion failure alerting with custom/default notification dispatch) and `laceBaseline` (rolling average monitoring with spike detection) without touching the core executor. Extensions compose via `require` dependencies and exposed functions.

Hurl has no extension system. Its feature set is fixed by the tool.

### Three-way separation: language, validator, executor.

The Lace ecosystem splits into three concerns:

- **Language spec** (`lacelang/`) — grammar, schemas, conformance vectors
- **Validator** (`lacelang-*-validator/`) — parsing and semantic checks, no HTTP
- **Executor** (`lacelang-*-executor/`) — HTTP client, assertion evaluation, extension processing

A `.lace` file is a portable artifact. It can be validated without executing, executed by any conformant executor, and its result is a standardized JSON structure that any backend can consume. The conformance test suite (190+ vectors) ensures identical behavior across implementations.

Hurl is a single binary. This is simpler, but it means you're coupled to one implementation, one release cycle, and one feature set.

### Backend integration by design.

Lace's `.store()` mechanism distinguishes run-scope variables (`$$var` — available within the current execution for call chaining) from write-back variables (`$var` and plain keys — emitted in the result for the backend to persist). Using `$var` as a write-back key signals the backend to update the injected variable for future runs without changing its in-memory value during the current execution. The result's `actions` object carries extension-emitted data (notifications, baseline stats, custom events) in a structured, typed format.

The executor is deliberately side-effect-free. It doesn't send emails, post to Slack, or write to databases. It returns a result. The monitoring platform that runs the probe decides what to do. This makes Lace usable inside any backend — from a cron job piping JSON to jq, to a distributed monitoring fleet with custom alerting rules.

### Timing as a first-class citizen.

Every call in Lace's result includes a full timing breakdown: DNS resolution, TCP connect, TLS handshake, time to first byte, body transfer, and total response time — plus DNS metadata (resolved IPs) and TLS metadata (protocol, cipher, certificate chain). These are available as assertion scopes (`.expect(dnsMs: 50)`) and as extension inputs (baseline spike detection operates on all timing fields).

Hurl captures duration but doesn't decompose it into phases. For monitoring, knowing _why_ a request was slow (DNS? TLS negotiation? Backend processing?) is as important as knowing _that_ it was slow.

## When to Use Hurl Instead

Hurl is the better choice when:

- You need quick HTTP smoke tests in CI
- You want a single self-contained binary with zero configuration
- Your assertions are pass/fail with no need for structured result analysis
- You don't need cross-run state, extension hooks, or timing decomposition
- You're testing, not monitoring

Lace is not trying to replace Hurl for development-time testing. It exists because production monitoring probes have requirements that testing tools weren't designed for.
