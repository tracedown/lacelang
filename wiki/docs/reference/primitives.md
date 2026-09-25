# Extension Primitives

Built-in primitive functions provided by the executor's rule interpreter (extension specification §7). Available in all extension rule bodies and functions -- not in `.lace` scripts; for the functions a script can call see [Helper Functions](helper-functions.md). All implementations provide these identically.

Spec version: 0.9.7<!-- sv -->

## `compare(a, b) -> string | null`

Returns the op key describing the actual relationship between `a` and `b`.

**Signature:** `compare(a, b) -> string | null`

| Condition | Returns |
|---|---|
| `a lt b` | `"lt"` |
| `a eq b` | `"eq"` |
| `a gt b` | `"gt"` |
| `a neq b` and not ordered (booleans, mismatched comparable types) | `"neq"` |
| Either operand is `null` | `null` |
| Operands are incomparable types | `null` |

`compare` describes one relationship, so it never returns `"lte"` or `"gte"` -- an `op_map` keyed on those never matches. For numbers and strings (lexicographic) the result is `"lt"`, `"eq"` or `"gt"`; for booleans `"eq"` or `"neq"`.

**Examples:**

```
compare(100, 200)     # -> "lt"
compare("abc", "abc") # -> "eq"
compare(true, false)  # -> "neq"
compare(null, 5)      # -> null
```

## `map_get(map, key) -> any | null`

Looks up `key` in `map`. Falls back to `map["default"]` if `key` is absent. Returns `null` if neither `key` nor `"default"` is present.

**Signature:** `map_get(map, key) -> any | null`

**Null handling:** Returns `null` if `map` is null.

**Examples:**

```
map_get({ "lt": "a", "default": "b" }, "gt")  # -> "b"
map_get({ "lt": "a" }, "gt")                  # -> null
map_get({ "eq": "a" }, "eq")                  # -> "a"
```

## `map_match(map, actual, expected, op) -> any | null`

Resolves the best matching key in a notification-style map for a validation failure. The `op` argument is accepted for call-site symmetry with the scope context but is not consulted. Tries the following in order, returning the first match:

1. The string representation of `actual` as a key (eq comparison -- matches literal value keys like `"404"`)
2. The result of `compare(actual, expected)` as a key (op key match)
3. `"default"` as a key

**Signature:** `map_match(map, actual, expected, op) -> any | null`

**Null handling:** Returns `null` if no key matches or if `map` is null.

**Examples:**

```
map_match({"404": t1, "gt": t2, "default": t3}, 404, 200, "eq")
# -> t1  (actual "404" matches literal key)

map_match({"lt": t2, "default": t3}, 1200, 500, "lt")
# compare(1200, 500) = "gt" -- no "gt" key
# -> t3  (default)

map_match({"lt": t4}, 100, 500, "lt")
# compare(100, 500) = "lt" -- matches "lt"
# -> t4
```

## `is_null(v) -> bool`

Returns `true` if `v` is `null`, `false` otherwise.

**Signature:** `is_null(v) -> bool`

**Examples:**

```
is_null(null)    # -> true
is_null(0)       # -> false
is_null("")      # -> false
```

## `type_of(v) -> string`

Returns the type name of `v`.

**Signature:** `type_of(v) -> string`

| Value | Returns |
|---|---|
| String | `"string"` |
| Integer | `"int"` |
| Float | `"float"` |
| Boolean | `"bool"` |
| Object/map | `"object"` |
| Array | `"array"` |
| Null | `"null"` |

**Examples:**

```
type_of("hello")  # -> "string"
type_of(42)       # -> "int"
type_of(null)     # -> "null"
type_of([1, 2])   # -> "array"
```

## `to_string(v) -> string`

Converts any value to its string representation.

**Signature:** `to_string(v) -> string`

| Input | Output |
|---|---|
| `null` | `"null"` |
| `true` / `false` | `"true"` / `"false"` |
| Numbers | Decimal form |
| Strings | Returns unchanged |

**Examples:**

```
to_string(42)     # -> "42"
to_string(null)   # -> "null"
to_string(true)   # -> "true"
```

## `replace(str, pattern, replacement) -> string | null`

Returns a copy of `str` with all occurrences of `pattern` replaced by `replacement`. The `replacement` value is converted to a string via `to_string()` before substitution.

**Signature:** `replace(str, pattern, replacement) -> string | null`

**Null handling:** If `str` or `pattern` is null, returns `str` unchanged (so a null `str` yields `null`).

**Examples:**

```
replace("hello $name", "$name", "world")    # -> "hello world"
replace("x=$val", "$val", 42)               # -> "x=42"
replace(null, "a", "b")                     # -> null
```
