# Failure Semantics

Every assertion in a Lace script produces one of three outcomes: **passed**, **failed**, or **indeterminate**. Failures are either **hard** (the rest of the script is skipped) or **soft** (recorded, execution continues).

## Hard Fail

A hard fail skips `.store()` and `.wait()` on the current call **and all subsequent calls** in the script. The current call's `.check()` and `.assert()` still run and are recorded. The result records every call that was skipped with outcome `"skipped"`.

**Sources of hard fail:**

| Source | Description |
|---|---|
| `.expect()` | Any scope fails (after all scopes are evaluated) |
| `.assert({ expect: [...] })` | Any expect condition fails (after all are evaluated) |
| Redirect limit exceeded | The call needs more than `redirects.max` hops --- not overridable |
| TLS error with `rejectInvalidCerts: true` | Any TLS error, e.g. an invalid certificate --- not overridable |
| `schema($var)` where `$var` is null | Missing schema variable |
| `timeout.action: "fail"` | Request timed out |
| All retries exhausted | `timeout.action: "retry"` with all retries failed |

A `redirects.max` or `timeout.ms` above the system maximum never gets this far: it is a validation error (`REDIRECTS_MAX_LIMIT` / `TIMEOUT_MS_LIMIT`) and the script does not run.

### Timeouts

A call that times out always gets the call outcome `"timeout"` (not `"failure"`), whatever its `timeout.action`. The action decides what happens next:

- `"fail"`, or `"retry"` once the retries are exhausted --- a hard fail. Later calls are skipped and the run outcome is `"timeout"`.
- `"warn"` --- a soft fail. The run continues, and the timeout does not change the run outcome.

The run outcome `"timeout"` therefore means that a call timed out and stopped the run; there is no separate run-level timeout.

## Soft Fail

A soft fail is recorded but execution continues normally through remaining chain methods and subsequent calls.

**Sources of soft fail:**

| Source | Description |
|---|---|
| `.check()` | Any scope fails (after all scopes are evaluated) |
| `.assert({ check: [...] })` | Any check condition fails (after all are evaluated) |
| `timeout.action: "warn"` | Request timed out but configured to warn only |
| TLS error with `rejectInvalidCerts: false` | The TLS error is recorded as a warning in the call record; the executor does not otherwise interpret the certificate |

## Indeterminate

An indeterminate outcome is neither pass nor fail. It occurs when a comparison cannot produce a meaningful result.

**Source:** a `null` operand in an ordered comparison (`lt`, `lte`, `gt`, `gte`) or in arithmetic (`+`, `-`, `*`, `/`, `%`).

```lace
// If $$prev_count is null (never stored), this is indeterminate
get("$BASE_URL/api/metrics")
.expect(status: 200)
.assert({
  check: [
    $$prev_count lt this.body.count
  ]
})
```

Indeterminate outcomes are recorded as `"indeterminate"` in the assertion record. Execution continues.

!!! note "Equality is not indeterminate"
    `null eq null` is `true`. `null eq 42` is `false`. `null neq 42` is `true`. Only ordered comparisons and arithmetic with null produce indeterminate results.

## Complete Evaluation Before Cascade

This is a key design principle: **all scopes/conditions are evaluated before any failure takes effect.**

`.expect()` evaluates every scope. `.assert({ expect: [...] })` evaluates every condition. Every assertion block on the call --- `.expect()`, `.check()`, `.assert()` --- is evaluated in full before the hard-fail cascade acts. This ensures all failures on a call are visible simultaneously in the result.

```lace
.expect(
  status: 200,        // fails
  totalDelayMs: 500,  // also fails
  body: schema($s)    // also evaluated
)
// All three results are recorded, THEN the hard fail cascade begins
```

## Cascade Rules

When a hard fail occurs:

1. **`.store()` and `.wait()` on the current call are skipped.** If `.expect()` fails, the call's `.check()` and `.assert()` are still evaluated and recorded; `.store()` and `.wait()` are not run.
2. **All subsequent calls are skipped.** They appear in the result with outcome `"skipped"`.

`.store()` is specifically skipped whenever a preceding chain method on the same call produced a hard fail. This prevents storing values from a response that failed validation.

## Summary Table

| Outcome | Sources | Effect on execution |
|---|---|---|
| **Hard fail** | `.expect()`, `.assert({ expect })`, redirect limit, TLS error (strict), `schema(null)`, timeout (fail/retry exhausted) | Skip `.store()`/`.wait()` on the call + all subsequent calls |
| **Soft fail** | `.check()`, `.assert({ check })`, timeout (warn), TLS error (lenient) | Record failure, continue |
| **Indeterminate** | `null` in ordered comparison or arithmetic | Record as indeterminate, continue |
| **Passed** | Assertion condition met | Continue |
