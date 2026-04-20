# Previous Results

The `prev` reference gives your script access to the result of the **previous run**. This lets you compare current behaviour against past behaviour --- detecting regressions, tracking trends, or suppressing duplicate notifications.

## Providing Previous Results

Previous results are loaded from a JSON file via the CLI or config:

```bash
lace run script.lace --vars vars.json --prev-results last_result.json
```

The file must contain a result JSON from a prior run of the same script (see the result structure in the spec).

## Access Syntax

`prev` mirrors the [result structure](../result/index.md). Access fields with dot notation and array indexing:

```lace
// Top-level outcome
prev.outcome                         // "success", "failure", "timeout", or null

// Run-scope variables from the previous run
prev.runVars.token
prev.runVars.count_before

// Individual call results
prev.calls[0].outcome                // "success", "failure", etc.
prev.calls[0].response.status        // HTTP status code
prev.calls[0].response.responseTimeMs

// Assertion results from the previous run
prev.calls[0].assertions[0].outcome  // "passed", "failed", "indeterminate"
prev.calls[0].assertions[0].actualLhs
```

## Null When Missing

When no previous results are provided, `prev` resolves to `null`. Any field access on `null` propagates `null` --- it does not throw an error.

```lace
// If --prev-results was not provided:
prev.outcome              // null
prev.calls[0].response    // null (null propagation)
prev.runVars.token        // null (null propagation)
```

This means you can safely reference `prev` without checking whether it exists. The null will propagate through to your assertion, where it follows the standard [null semantics](variables.md#null-semantics):

- `prev.outcome eq null` --- `true` (no previous results)
- `prev.outcome eq "success"` --- `false` (null neq any value)
- `prev.calls[0].response.status lt 300` --- **indeterminate** (null in ordered comparison)

## Practical Examples

### Compare response time against previous run

```lace
get("$BASE_URL/api/search")
.expect(status: 200)
.assert({
  check: [
    // Warn if response time is more than double the previous run
    this.responseTime lt (prev.calls[0].response.responseTimeMs * 2)
  ]
})
```

### Check if the previous run failed

```lace
get("$BASE_URL/health")
.expect(status: 200)
.assert({
  check: [
    // Soft-check: was the previous run also successful?
    prev.outcome eq "success"
  ]
})
```

### Use a previous run-scope variable

```lace
get("$BASE_URL/api/metrics")
.expect(status: 200)
.store({ "$$current_count": this.body.count })
.assert({
  check: [
    // Verify count hasn't decreased since last run
    $$current_count gte prev.runVars.current_count
  ]
})
```

!!! note "Validator warning"
    The validator emits a warning if your script uses `prev` references but `--prev-results` was not provided. The script will still run --- `prev` will just be `null`.
