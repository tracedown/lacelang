# Custom Assertions: .assert()

`.assert()` lets you write arbitrary conditions using expressions, going beyond the built-in scopes of `.expect()` and `.check()`. Conditions are grouped into `expect` (hard fail) and `check` (soft fail) arrays.

## Basic Structure

```lace
.assert({
  expect: [
    this.body.success eq true,
    this.body.items.length gt 0
  ],
  check: [
    this.responseTime lt $sla_ms
  ]
})
```

You can include `expect`, `check`, or both. Each array can have one or more conditions.

## Condition Forms

### Shorthand --- expression only

Write the condition directly in the array:

```lace
.assert({
  expect: [
    this.body.count eq 42,
    $$count_after eq $$count_before + 1
  ]
})
```

### Full form --- with options

Wrap the condition in an object to attach extension options:

```lace
.assert({
  expect: [
    {
      condition: $$count_after eq $$count_before + 1,
      options: {
        // Extension fields (e.g. notification templates)
      }
    }
  ]
})
```

You can mix shorthand and full form in the same array:

```lace
.assert({
  expect: [
    this.body.success eq true,
    {
      condition: $$count_after eq $$count_before + 1,
      options: {}
    }
  ]
})
```

## Expressions

Conditions are full expressions supporting:

**Comparison operators** (keywords, not symbols):

- `eq` --- equal
- `neq` --- not equal
- `lt` --- less than
- `lte` --- less than or equal
- `gt` --- greater than
- `gte` --- greater than or equal

**Arithmetic:** `+`, `-`, `*`, `/`, `%`

**Logical connectives:** `and`, `or`, `not`

**Null checks:** `eq null`, `neq null`

**Parentheses** for grouping:

```lace
.assert({
  expect: [
    (this.body.count gt 0) and (this.body.count lt 1000)
  ]
})
```

!!! warning "Comparisons do not chain"
    `a eq b eq c` is a parse error. Write `(a eq b) and (b eq c)` instead.

### Available references in expressions

- `this.*` --- current response fields
- `prev.*` --- previous run results
- `$var` --- script variables
- `$$var` --- run-scope variables
- `json()`, `form()`, `schema()` --- helper functions

## Complete Evaluation

Like `.expect()`, all `expect` conditions are evaluated before any hard fail triggers. This means you see every failing condition at once:

```lace
.assert({
  expect: [
    this.body.success eq true,       // evaluated even if the next one fails
    $$count_after eq $$count_before + 1
  ]
})
```

All `check` conditions are always evaluated regardless of failures.

## Indeterminate Outcomes

When a comparison involves a `null` operand in an ordered operator (`lt`, `lte`, `gt`, `gte`) or in arithmetic, the result is **indeterminate** --- not a pass, not a fail. It is recorded as `"indeterminate"` and execution continues.

```lace
// If $$previous_count was never stored (null), this is indeterminate, not a failure
.assert({
  check: [
    $$previous_count lt this.body.count
  ]
})
```

Equality checks with null work normally: `null eq null` is `true`, `null eq 42` is `false`.

See [Failure Semantics](failure-semantics.md) for the full rules.
