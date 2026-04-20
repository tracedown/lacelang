# Failure Semantics

Every assertion in a Lace script produces one of three outcomes: **passed**, **failed**, or **indeterminate**. Failures are either **hard** (execution stops) or **soft** (recorded, execution continues).

## Hard Fail

A hard fail skips all remaining chain methods on the current call **and all subsequent calls** in the script. The result records every call that was skipped with outcome `"skipped"`.

**Sources of hard fail:**

| Source | Description |
|---|---|
| `.expect()` | Any scope fails (after all scopes are evaluated) |
| `.assert({ expect: [...] })` | Any expect condition fails (after all are evaluated) |
| Redirect limit exceeded | `redirects.max` reached --- not overridable |
| TLS error with `rejectInvalidCerts: true` | Invalid certificate --- not overridable |
| `schema($var)` where `$var` is null | Missing schema variable |
| `timeout.action: "fail"` | Request timed out |
| All retries exhausted | `timeout.action: "retry"` with all retries failed |

## Soft Fail

A soft fail is recorded but execution continues normally through remaining chain methods and subsequent calls.

**Sources of soft fail:**

| Source | Description |
|---|---|
| `.check()` | Any scope fails (after all scopes are evaluated) |
| `.assert({ check: [...] })` | Any check condition fails (after all are evaluated) |
| `timeout.action: "warn"` | Request timed out but configured to warn only |
| TLS error with `rejectInvalidCerts: false` | Invalid certificate, configured to allow |

## Indeterminate

An indeterminate outcome is neither pass nor fail. It occurs when a comparison cannot produce a meaningful result.

**Source:** a `null` operand in an ordered comparison (`lt`, `lte`, `gt`, `gte`) or in arithmetic (`+`, `-`, `*`, `/`, `%`).

```lace
// If $$prev_count is null (never stored), this is indeterminate
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

`.expect()` evaluates every scope. `.assert({ expect: [...] })` evaluates every condition. Only after the full evaluation does the hard-fail cascade trigger. This ensures all failures are visible simultaneously in the result.

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

1. **Remaining chain methods on the current call are skipped.** If `.expect()` fails, then `.check()`, `.assert()`, `.store()`, and `.wait()` on that call are all skipped.
2. **All subsequent calls are skipped.** They appear in the result with outcome `"skipped"`.

`.store()` is specifically skipped whenever a preceding chain method on the same call produced a hard fail. This prevents storing values from a response that failed validation.

## Summary Table

| Outcome | Sources | Effect on execution |
|---|---|---|
| **Hard fail** | `.expect()`, `.assert({ expect })`, redirect limit, TLS error (strict), `schema(null)`, timeout (fail/retry exhausted) | Skip remaining chain methods + all subsequent calls |
| **Soft fail** | `.check()`, `.assert({ check })`, timeout (warn), TLS error (lenient) | Record failure, continue |
| **Indeterminate** | `null` in ordered comparison or arithmetic | Record as indeterminate, continue |
| **Passed** | Assertion condition met | Continue |
