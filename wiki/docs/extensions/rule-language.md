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

The `on` array lists which [hooks](hooks.md) trigger this rule. A rule can fire on multiple hooks.

## Statement reference

### `for $binding in expr:`

Iterates over an array. If the expression evaluates to `null`, the loop body is skipped entirely. The binding is available only within the loop body.

```
for $call in result.calls:
  for $a in $call.assertions:
    when $a.outcome eq "failed"
    emit result.actions.notifications <- {
      callIndex: $call.index,
      conditionIndex: $a.index,
      trigger: "assert",
      notification: structured({ kind: $a.kind })
    }
```

### `when expr` (inline guard)

When the expression is false or null, the block of statements following the guard is skipped. A block runs from the next line up to the next blank line or the end of the enclosing scope.

```
when scope.outcome eq "failed"
when not is_null($notif_cfg)
let $notif = resolve_scope_notif($notif_cfg, scope.actual, scope.value, scope.op)
```

Multiple inline guards chain by nesting -- each successive `when` is inside the previous guard's block, so all must pass:

```
when $a.outcome eq "failed"
when $a.options neq null
when not is_null($a.options.notification)
// all three guards passed
```

### `when expr:` (block form)

Explicit block form with an indented body. When the expression is false or null, the indented block is skipped. Execution continues after the block.

```
when $call.response neq null:
  let $status = $call.response.status
  // $status only used here
// execution continues here regardless
```

### `let $binding = expr`

Binds a name to a value. Immutable within the current scope -- the same name cannot be rebound.

```
let $prev_scope = prev?.calls[call.index]?.assertions[? $.scope eq scope.name]
let $silent = is_silent(scope.options, $prev_scope?.outcome)
```

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

### `exit`

**Rule bodies only.** Exits the current rule body immediately. Not valid in functions (use `return` there).

### `return expr`

**Function bodies only.** Exits the function and produces a value. Not valid in rule bodies (use `exit` there). A function that reaches the end without a `return` returns `null`.

```
when is_null(notif_cfg)
return null

when notif_cfg.tag eq "template" or notif_cfg.tag eq "text"
return notif_cfg

return map_match(notif_cfg.ops, actual, expected, op)
```

## Statement availability summary

| Statement | Rule bodies | Function bodies |
|---|---|---|
| `for` | Yes | Yes |
| `when` | Yes | Yes |
| `let` | Yes | Yes |
| `set` | No (parse error) | Yes |
| `emit` | Yes | Only in exposed functions |
| `exit` | Yes | No (use `return`) |
| `return` | No (parse error) | Yes |
