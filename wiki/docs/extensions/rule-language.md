# Rule Language

Extension rules are written in a small, indentation-aware language embedded in TOML string values. The language is the same across all executor implementations.

## Rule structure

Rules are declared as entries in the `[[rules.rule]]` array. Each rule has a name, one or more hook points, and a body:

```toml
[[rules.rule]]
name = "scope_expect_default_notifications"
on   = ["expect"]
body = """
when scope.outcome eq "failed"
when is_null(scope.options?.notification)
let $prev_scope = prev?.calls[call.index]?.assertions[? $.scope eq scope.name]
let $silent = is_silent(scope.options, $prev_scope?.outcome)
when not $silent
emit result.actions.notifications <- {
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "expect",
  scope:           scope.name,
  notification:    structured({
    scope:    scope.name,
    op:       scope.op,
    expected: scope.value,
    actual:   scope.actual
  })
}
"""
```

The `on` array lists which [hooks](hooks.md) trigger this rule. A rule can fire on multiple hooks. After-hooks are named by the block (`"call"`, `"expect"`, `"script"`, ...); before-hooks take a `before ` prefix (`on = ["before call"]`). An entry may also carry ordering qualifiers such as `"call after laceNotifications"` -- see [Rule ordering](hooks.md#rule-ordering).

**Comments** in rule and function bodies start with `#`. There is no `//` comment form.

## Statement reference

### `for $binding in expr:`

Iterates over an array. If the expression evaluates to `null`, the loop body is skipped entirely. The binding is available only within the loop body.

```
for $call in result.calls:
  for $a in $call.assertions:
    when $a.method eq "assert" and $a.outcome eq "failed"
    emit result.actions.notifications <- {
      callIndex: $call.index,
      conditionIndex: $a.index,
      trigger: "assert",
      notification: structured({ kind: $a.kind })
    }
```

### `when expr` (inline guard)

When the expression is false or null, the block of statements following the guard is skipped and execution continues after the block. A block runs from the next line up to (but not including) the next **blank line** or the end of the enclosing scope (function body, `for` body, `when` block, or rule body).

Inline `when X` is sugar for the block form with the following non-blank lines as its body:

```
when X
STMT_A
STMT_B

STMT_C
```

is equivalent to:

```
when X:
    STMT_A
    STMT_B

STMT_C
```

The blank line closes the guard's block; `STMT_C` runs unconditionally.

```
when scope.outcome eq "failed"
let $notif_cfg = scope.options?.notification
when not is_null($notif_cfg)
let $notif = resolve_scope_notif($notif_cfg, scope.actual, scope.value, scope.op)
```

Multiple inline guards chain by nesting -- each successive `when` is inside the previous guard's block, so all must pass:

```
when $a.outcome eq "failed"
when $a.options neq null
when not is_null($a.options.notification)
# all three guards passed
```

### `when expr:` (block form)

Explicit block form with an indented body. When the expression is false or null, the indented block is skipped. Execution continues after the block.

```
when $call.response neq null:
  let $status = $call.response.status
  # $status only used here
# execution continues here regardless
```

### `let $binding = expr`

Binds a name to a value. Immutable -- the same name cannot be rebound in the current scope or in any enclosing scope (a `let` inside an inline-`when` block cannot shadow an outer binding; rebinding is a runtime error). Use a ternary to pick between values instead:

```
let $prev_scope = prev?.calls[call.index]?.assertions[? $.scope eq scope.name]
let $silent = is_silent(scope.options, $prev_scope?.outcome)
let $raw = config.min_entries
let $min = is_null($raw) ? 5 : $raw
```

A new `for` iteration starts a fresh scope, so a `let` inside a loop body binds anew on each iteration.

`$`-prefixed names are only for `let` / `for` bindings. Hook context objects (`call`, `scope`, `condition`, `entry`, `script`) and function parameters are bare identifiers.

### `set $binding = expr`

**Function bodies only.** Reassigns an existing binding created by a prior `let`. Walks up the scope chain to find the binding. Using `set` in a rule body is a parse error. Using `set` on an unbound name is a runtime error.

```
let $sum = 0
for $item in items:
  set $sum = $sum + $item.value
return $sum
```

### `emit target <- { fields }`

Appends an object to a result array or merges into `runVars`. The target must be a path registered in `[result]` or `result.runVars`.

```
emit result.actions.notifications <- {
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "expect",
  scope:           scope.name,
  notification:    $notif
}
```

```
emit result.runVars <- {
  "laceBaseline.stats": $new_stats
}
```

### Call statement: `fn(...)` / `ext.fn(...)`

A function call on its own line is a statement: the function runs for its side effects and its return value is discarded. The local form calls a function from the same extension; the qualified form calls an [exposed function](functions.md#local-vs-exposed-functions) of a `require`d extension -- typically one that `emit`s on behalf of its owner:

```
laceNotifications.pushNotification({
  callIndex:      -1,
  conditionIndex: -1,
  trigger:        "recovered",
  scope:          null,
  notification:   $notif
})
```

`laceBaseline` uses the local form to run one spike check per metric: `check_spike($stats, "dnsMs", $resp.dnsMs, call.index, $mult)`.

### `exit`

**Rule bodies only.** Exits the current rule body immediately. Not valid in functions (use `return` there).

### `return expr`

**Function bodies only.** Exits the function and produces a value. Not valid in rule bodies (use `exit` there). A function that reaches the end without a `return` returns `null`.

```
when is_null(notif_cfg)
return null

when is_concrete(notif_cfg)
return notif_cfg

return map_match(notif_ops(notif_cfg), actual, expected, op)
```

## Statement availability summary

| Statement | Rule bodies | Function bodies |
|---|---|---|
| `for` | Yes | Yes |
| `when` | Yes | Yes |
| `let` | Yes | Yes |
| `set` | No (parse error) | Yes |
| `emit` | Yes | Only in exposed functions |
| call statement (`fn(...)`, `ext.fn(...)`) | Yes | Yes |
| `exit` | Yes | No (use `return`) |
| `return` | No (parse error) | Yes |
