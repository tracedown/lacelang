# Language Overview

A `.lace` script defines one or more HTTP calls that run in sequence. Each call has **chain methods** that validate the response, store values, run custom assertions, or pause before the next call.

## What a Script Looks Like

```lace
get("$BASE_URL/health")
.expect(status: 200)
.check(totalDelayMs: 500)
```

That is a complete script: one GET request, one hard assertion on the status code, and one soft check on response time.

Scripts grow by adding more calls and more chain methods:

```lace
// Log in and capture a token
post("$BASE_URL/login", {
  body: json({ email: "$admin_email", password: "$admin_pass" })
})
.expect(status: 200)
.store({ "$$token": this.body.access_token })

// Use the token to fetch a protected resource
get("$BASE_URL/api/dashboard", {
  headers: { Authorization: "Bearer $$token" }
})
.expect(
  status: 200,
  body: schema($dashboard_schema)
)
.check(totalDelayMs: { value: 1000 })

// Verify a side-effect
get("$BASE_URL/api/metrics")
.expect(status: 200)
.store({ "$$visit_count": this.body.visits })
.assert({
  expect: [$$visit_count gt 0]
})
```

## Execution Model

Calls run **top to bottom, one at a time**. After the HTTP response arrives, the call's chain methods execute in a fixed order:

1. **`.expect()`** --- hard-fail assertions on the response (status, timing, body, headers)
2. **`.check()`** --- soft-fail assertions (same syntax, but failures are recorded and execution continues)
3. **`.assert()`** --- custom expression-based conditions (hard or soft)
4. **`.store()`** --- save values from the response for later calls or for the backend
5. **`.wait()`** --- pause for a number of milliseconds before the next call

You can use any subset of these, but the order is always the same. Each method appears at most once per call.

!!! warning "Chain method order is enforced"
    Writing `.store()` before `.expect()` is a validation error. The order is always: expect, check, assert, store, wait.

## Hard Fails vs. Soft Fails

When `.expect()` or `.assert({ expect: [...] })` fails, it is a **hard fail**: remaining chain methods on that call and all subsequent calls are skipped. You see every failing scope at once (all scopes evaluate before the cascade triggers), but execution stops after that call.

When `.check()` or `.assert({ check: [...] })` fails, it is a **soft fail**: the failure is recorded, but execution continues normally.

## Sub-Pages

| Page | What it covers |
|---|---|
| [HTTP Calls](http-calls.md) | Methods, URLs, request config, cookies, timeouts |
| [Assertions](assertions.md) | `.expect()` and `.check()` scopes and operators |
| [Custom Assertions](custom-assertions.md) | `.assert()` with expression conditions |
| [Variables](variables.md) | `$var`, `$$var`, `.store()`, interpolation, null semantics |
| [Request Chaining](chaining.md) | Multi-call scripts, passing data between calls |
| [Previous Results](previous-results.md) | `prev` reference for cross-run comparisons |
| [Failure Semantics](failure-semantics.md) | Hard fail, soft fail, indeterminate, cascade rules |
| [Configuration](configuration.md) | `lace.config` TOML, environment variables, CLI flags |
