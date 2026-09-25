# Functions

Functions are defined in the `[functions]` section of a `.laceext` file and called from rule bodies and other functions.

## Defining functions

```toml
[functions.resolve_scope_notif]
params = ["notif_cfg", "actual", "expected", "op"]
body   = """
when is_null(notif_cfg)
return null

when is_concrete(notif_cfg)
return notif_cfg

return map_match(notif_ops(notif_cfg), actual, expected, op)
"""
```

- **`params`** -- list of parameter names. Parameters are bound as **bare identifiers** and read like hook context fields: `notif_cfg`, not `$notif_cfg`. `$`-prefixed names are reserved for `let` / `for` bindings.
- **`body`** -- the function body as a multi-line string using the same rule language.
- **`exposed`** -- optional, `true` makes the function callable from other extensions (see below).

## Function rules

- `return expr` exits the function and produces a value.
- A function that reaches the end without `return` returns `null`.
- `exit` is not valid in functions -- use `return null` for early termination.
- `set $name = expr` is valid in functions for reassigning bindings (not available in rule bodies).
- Functions may call other functions in the same extension, built-in primitives and [tag constructors](#tag-constructors).
- A call on its own line is a statement -- the function runs for its side effects and the return value is discarded.
- **No recursion.** The call graph must be a DAG.
- Functions are **pure by default** -- they cannot `emit` or access `result` directly. Exception: exposed functions (see below).

## Local vs exposed functions

By default, functions are local to the extension. Add `exposed = true` to make a function callable from other extensions:

```toml
[functions.pushNotification]
params  = ["event"]
exposed = true
body = """
emit result.actions.notifications <- {
  callIndex:      event.callIndex,
  conditionIndex: event.conditionIndex,
  trigger:         event.trigger,
  scope:           event.scope,
  notification:    event.notification
}
return event
"""
```

### Calling exposed functions

Other extensions that declare the owning extension in their `require` can call the function using qualified syntax:

```
laceNotifications.pushNotification({
  callIndex:      call_index,
  conditionIndex: -1,
  trigger:         "baseline_spike",
  scope:           metric_name,
  notification:    structured({
    metric:     metric_name,
    actual:     actual,
    average:    $avg,
    threshold:  $threshold,
    multiplier: multiplier
  })
})
```

(from `laceBaseline`'s exposed `check_spike` function, whose parameters are `stats`, `metric_name`, `actual`, `call_index` and `multiplier`)

The format is `extensionName.functionName(args)`.

### Exposed function execution model

The exposed function executes inside the **owning** extension's interpreter:

- `emit` attributes to the owner -- entries go into the owner's declared result paths.
- `result.runVars` emits use the owner's namespace prefix, not the caller's.
- The owner's `require` view, primitives, tag constructors and internal functions are in scope, not the caller's.
- Exposed functions still accept and return values like any other function.

### Restrictions

- Calling a non-exposed function from another extension raises `is not an exposed function`.
- The caller must list the owning extension in its `require`. Omitting it is a runtime error when the call fires.
- Cross-extension recursion is forbidden -- `extA.foo` cannot call `extB.bar` that calls back into `extA.foo`.

## Tag constructors

Every `one_of` variant declared in an extension's `[types]` section is callable by its tag -- from `.lace` option values and from rule bodies. The call takes the variant's fields as positional arguments and produces `{ tag: "<tag>", <field>: ... }`. With `laceNotifications` loaded:

| Constructor | Produces |
|---|---|
| `text("Service down")` | `{ tag: "text", value: "Service down" }` |
| `template("high-severity")` | `{ tag: "template", name: "high-severity" }` |
| `structured({ error: call.error })` | `{ tag: "structured", data: { error: ... } }` |
| `op_map({ "gt": text("slow"), "default": text("?") })` | `{ tag: "op_map", ops: { ... } }` |

## Primitives

Built-in functions provided by the executor. Available in all rule bodies and functions. All implementations provide these identically.

### `compare(a, b) -> string | null`

Returns the op key describing the actual relationship between `a` and `b`:

| Condition | Returns |
|---|---|
| `a lt b` | `"lt"` |
| `a eq b` | `"eq"` |
| `a gt b` | `"gt"` |
| `a neq b` for booleans (not ordered) | `"neq"` |
| Either is `null` | `null` |
| Incomparable or mismatched types (e.g. int vs string, bool vs int) | `null` |

`compare` describes one relationship, so it never returns `"lte"` or `"gte"` -- an `op_map` keyed on those never matches. For numbers and strings (lexicographic) the result is `"lt"`, `"eq"` or `"gt"`; for booleans `"eq"` or `"neq"`.

```
compare(100, 500)   # "lt"
compare("a", "a")   # "eq"
compare(true, false) # "neq"
```

### `map_get(map, key) -> any | null`

Looks up `key` in `map`. Falls back to `map["default"]` if `key` is absent. Returns `null` if neither exists or if `map` is null.

```
map_get({ "lt": "below", "default": "other" }, "gt")  # "other"
map_get({ "lt": "below" }, "gt")                       # null
map_get({ "eq": "match" }, "eq")                       # "match"
```

### `map_match(map, actual, expected, op) -> any | null`

Resolves the best matching key in a map for a validation failure. The `op` argument is accepted for call-site symmetry with the scope context but is not consulted. Tries in order:

1. The string representation of `actual` as a key (literal match, e.g. `"404"`)
2. The result of `compare(actual, expected)` as a key (op key match)
3. `"default"` as a key

Returns `null` if no key matches or if `map` is null.

```
map_match({"404": t1, "gt": t2, "default": t3}, 404, 200, "eq")
# t1 -- actual "404" matches literal key

map_match({"lt": t2, "default": t3}, 1200, 500, "lt")
# t3 -- compare(1200, 500) = "gt", no "gt" key, falls to default

map_match({"lt": t4}, 100, 500, "lt")
# t4 -- compare(100, 500) = "lt", matches
```

### `is_null(v) -> bool`

Returns `true` if `v` is `null`, `false` otherwise.

```
when is_null(scope.options?.notification)
# handle missing notification
```

### `type_of(v) -> string`

Returns the type name of `v`:

| Value | Returns |
|---|---|
| String | `"string"` |
| Integer | `"int"` |
| Float | `"float"` |
| Boolean | `"bool"` |
| Object/map | `"object"` |
| Array | `"array"` |
| Null | `"null"` |

### `to_string(v) -> string`

Converts any value to its string representation. Null returns `"null"`, booleans return `"true"` / `"false"`, numbers return their decimal form, strings return themselves.

### `replace(str, pattern, replacement) -> string | null`

Returns a copy of `str` with all occurrences of `pattern` replaced by `replacement`. The `replacement` is converted via `to_string()` before substitution. If `str` or `pattern` is null, returns `str` unchanged (so `null` for a null `str`).

```
replace("hello $name", "$name", "world")  # "hello world"
replace("x=$val", "$val", 42)             # "x=42"
replace(null, "a", "b")                   # null
replace("abc", null, "x")                 # "abc"
```
