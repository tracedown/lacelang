# Expressions

Expressions are used on the right side of `let`/`set` assignments, in `when` conditions, as `emit` field values, and as function arguments.

## Field access

Use dot notation to access fields on objects. **All field access is null-safe by default** -- accessing any field on `null` returns `null` rather than throwing.

```
$call.response.status          # null if response is null
$call.response?.status         # identical -- explicit null-safe marker
```

The `?.` operator is purely for readability. Both `.` and `?.` behave the same way: they return `null` when the left side is `null`.

### Base access roots

| Root | Description |
|------|-------------|
| `$name` | A `let` or `for` binding |
| bare identifier | A hook context object (`call`, `scope`, `condition`, `entry`, `script`) or a function parameter -- never `$`-prefixed |
| `result` | The run result object |
| `prev` | Previous run result (null if unavailable) |
| `this` | Current response (available in scope, assert and store hooks) |
| `config` | Extension configuration values |
| `require` | Required extensions' `runVars` -- `require["dep"]["dep.key"]` (see [Variables & Config](variables-and-config.md#reading-another-extensions-variables)) |
| `null` | Null literal |
| `true` / `false` | Boolean literals |

## Array indexing

`[n]` returns the element at index n, or `null` if out of bounds.

```
result.calls[0]                # first call
result.calls[0].assertions[2]  # third assertion of first call
```

## Map access by key

With a string operand, `[ "key" ]` looks up a map key. This is how keys that are not identifiers -- such as dotted `runVars` names -- are read:

```
prev.runVars["laceBaseline.stats"]
require["laceNotifications"]["laceNotifications.suppressedCount"]
```

## Array filtering

`[? condition]` returns the **first** element for which the condition is true, or `null`. Within the condition, `$` refers to the current element.

```
result.calls[? $.outcome eq "failed"]             # first failed call
$call.assertions[? $.scope eq "status"]           # first status assertion
prev?.calls[call.index]?.assertions[? $.scope eq scope.name]
```

## Operators

### Arithmetic

| Operator | Description |
|----------|-------------|
| `+` | Addition (int/float) or string concatenation |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division (division by zero returns `null`) |
| `-` (unary) | Negation |

There is no `%` (modulo) operator. Integer arithmetic when both operands are integers; float when either is a float. String `+` string produces string concatenation.

### Comparison

| Operator | Description |
|----------|-------------|
| `eq` | Equal |
| `neq` | Not equal |
| `lt` | Less than |
| `lte` | Less than or equal |
| `gt` | Greater than |
| `gte` | Greater than or equal |

Comparison operators do not chain. `a eq b eq c` is a parse error; use `(a eq b) and (b eq c)`.

Both operands of ordered comparisons (`lt`, `lte`, `gt`, `gte`) must be the same comparable type (int, float, or string). Mixed types return `null`.

### Logical

| Operator | Description |
|----------|-------------|
| `and` | Logical AND (short-circuit) |
| `or` | Logical OR (short-circuit) |
| `not` | Logical negation |

### Ternary

```
expr ? true_value : false_value
```

Standard conditional expression. Evaluates the condition, returns the appropriate branch.

```
let $count = is_null(prev_stats) ? 0 : prev_stats.count
let $skip = spike_action eq "skip" ? check_any_spike(prev_stats, $call.response, spike_multiplier) : false
```

## Operator precedence

Highest to lowest:

1. Primary / field access
2. Unary `not`, unary `-`
3. `*`, `/`
4. `+`, `-`
5. `lt`, `lte`, `gt`, `gte` (non-chaining)
6. `eq`, `neq` (non-chaining)
7. `and`
8. `or`
9. Ternary `? :`

Ordered comparisons bind tighter than equality, so `a lt b eq true` reads as `(a lt b) eq true`.

## Object literals

Inline object literals are used as function arguments and emit field values:

```
structured({
  scope:    scope.name,
  op:       scope.op,
  expected: scope.value,
  actual:   scope.actual
})
```

Keys can be identifiers or strings. The empty object `{}` is valid.

## Null propagation rules

The extension language has no exceptions. Null propagates predictably:

| Operation | Result |
|-----------|--------|
| Field access on `null` | `null` |
| Array index on `null` | `null` |
| Array filter on `null` | `null` |
| Arithmetic with `null` operand | `null` |
| Ordered comparison with `null` operand | `null` |
| `null eq null` | `true` |
| `null neq null` | `false` |
| `not null` | `true` (null is falsy) |
| `for $x in null:` | Loop body skipped |
| `when null` | Guard fails (null is falsy) |

## No implicit type coercion

Operations between incompatible types return `null`. There is no automatic conversion between types.

| Operation | Left | Right | Result |
|---|---|---|---|
| `+` | int | int | int |
| `+` | float | float | float |
| `+` | int | float | float |
| `+` | string | string | string |
| `+` | any other | any | null |
| `lt`/`gt`/`lte`/`gte` | numeric | numeric | bool |
| `lt`/`gt`/`lte`/`gte` | string | string | bool (lexicographic) |
| `lt`/`gt`/`lte`/`gte` | other | any | null |
| `eq`/`neq` | any | any | bool |
