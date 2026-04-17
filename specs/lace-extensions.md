# Lace — Extension System v0.9.0

> Status: Initial release (v0.9.0)
> Referenced by: lace-spec.md §10

---

## Table of Contents

1. [Overview](#1-overview)
2. [Extension File Format](#2-extension-file-format)
   - 2.3 [Extension Config File](#23-extension-config-file)
3. [Schema Additions](#3-schema-additions)
   - 3.1 [Registration Targets](#31-registration-targets)
   - 3.2 [Type System](#32-type-system)
4. [Result Additions](#4-result-additions)
5. [Rule Body Language](#5-rule-body-language)
   - 5.1 [Formal Grammar](#51-formal-grammar)
   - 5.2 [Statements](#52-statements)
   - 5.3 [Expressions](#53-expressions)
   - 5.4 [Types and Type Coercion](#54-types-and-type-coercion)
   - 5.5 [Null Propagation](#55-null-propagation)
6. [Functions](#6-functions)
   - 6.1 [Exposed Functions](#61-exposed-functions)
7. [Primitives](#7-primitives)
8. [Hook Points](#8-hook-points)
   - 8.1 [Hook Execution Model](#81-hook-execution-model)
   - 8.2 [`on before script` / `on script`](#82-on-before-script--on-script)
   - 8.3 [`on before call` / `on call`](#83-on-before-call--on-call)
   - 8.4 [`on before expect` / `on expect`](#84-on-before-expect--on-expect)
   - 8.5 [`on before check` / `on check`](#85-on-before-check--on-check)
   - 8.6 [`on before assert` / `on assert`](#86-on-before-assert--on-assert)
   - 8.7 [`on before store` / `on store`](#87-on-before-store--on-store)
9. [Extension Variables](#9-extension-variables)
10. [Emit Targets](#10-emit-targets)
11. [Configuration](#11-configuration)
12. [Reference Extension: `laceNotifications`](#12-reference-extension-lacenotifications)
13. [Executor Compatibility Checklist](#13-executor-compatibility-checklist) → [checklist-extensions.md](./checklist-extensions.md)

---

## 1. Overview

Extensions add functionality to Lace without modifying the core language. An extension is a `.laceext` file — a TOML document containing schema additions, result additions, and rules. The executor's built-in extension processor interprets `.laceext` files using the rule body language defined in this document.

**Design principles:**

- Extensions are declarative. No imperative code runs in the executor from an extension — only the rule language defined here.
- The rule language is the same across all three executor implementations. An extension written once runs identically in Python, JavaScript, and Kotlin executors.
- Extensions are isolated. They may only write to their own namespace in `runVars`. They cannot modify `calls`, `outcome`, or `runVars` outside their prefix.
- The core executor never depends on any specific extension. `laceNotifications` and `laceLogging` ship bundled with the executor as `.laceext` files but are inactive unless listed in `lace.config`.

---

## 2. Extension File Format

A `.laceext` file is a TOML document with four top-level sections:

```toml
[extension]
name    = "laceNotifications"    # camelCase identifier — see naming rule below
version = "1.0.0"
require = []                     # optional — see §2.2

[schema]
# Schema additions — see §3

[result]
# Result additions — see §4

[functions]
# Function definitions — see §6

[rules]
# Rule definitions — see §5 and §8
```

All sections except `[extension]` are optional. An extension with no rules but with schema additions is a pure schema-extension. An extension with no schema additions but with rules is a pure post-processing extension.

**Extension name constraint.** `[extension].name` must match `[a-z][A-Za-z0-9]*` — a lowercase-leading camelCase identifier. No hyphens, underscores, or other punctuation. This keeps qualified function calls (`extName.fnName(...)`, §6.1) parseable as a single IDENT on each side of the dot without any adjacency heuristics.

**Loading:** the executor loads `.laceext` files listed in `lace.config` at startup. If a listed extension file is not found, startup fails with a clear error message. Extensions are loaded in the order listed; rules from later extensions run after rules from earlier extensions at the same hook point.

### 2.2 Dependencies (`require`)

An extension may declare other extensions it depends on:

```toml
[extension]
name    = "myExtension"
version = "1.0.0"
require = ["laceNotifications", "someOtherExt"]
```

Semantics:

- **Presence check only** — every name in `require` must be a loaded extension. If any required extension is absent (not listed in `lace.config`), the executor fails startup with a clear error naming the missing dep and who required it. Load order between the depending extension and its dependencies **does not matter**.
- **Read access** — the depending extension's rule bodies and functions gain read access to the `result.runVars` entries emitted by the required extensions in the current run. See §9.1.
- **No write access** — a required extension's runVars are still write-locked to their owner; the depending extension can read but not mutate.
- **No transitive read** — if A requires B and B requires C, A does **not** automatically gain read access to C. A must name C explicitly in its own `require` list.
- **Implicit `after` default** — at any hook where a required extension has rules, the depending extension's rules on the **same** hook are scheduled to run after them by default (see §8.1). At hooks where the required extension has no rules, no edge is added — the depending extension runs unimpeded.
- **Acyclic per hook** — a cycle among rules at a single hook is a startup error, with the offending edge chain reported.
- **Version negotiation is out of scope for the initial release** — `require` names the extension only; version compatibility is tracked separately.

### 2.3 Extension Config File

An extension may ship a companion config file that declares default configuration values. The config file lives alongside the `.laceext` file and uses the naming convention `{extName}.config`.

**Naming and location.** For an extension file `laceNotifications.laceext`, the config file is `laceNotifications.config` in the same directory. The loader automatically looks for a sibling `.config` file when loading a `.laceext` file. If no `.config` file is found, the extension has no defaults — this is fully backward compatible with extensions that predate config files.

**Format.** The config file is a TOML document with two sections:

```toml
[extension]
name    = "laceNotifications"    # must match the .laceext file's name
version = "1.0.0"               # must match the .laceext file's version

[config]
channel      = "general"
max_retries  = 3
include_body = true
```

The `[extension]` header is validated at load time — if `name` or `version` differs from the `.laceext` file, the executor fails startup with a clear mismatch error.

The `[config]` section contains key-value pairs that become the default values for the extension's `config` namespace (accessible in rule bodies and functions as `config.{key}`).

**Example.** Given the config file above, a rule body may reference `config.channel` and receive `"general"` unless the project's `lace.config` overrides it:

```
let $ch = config.channel          # "general" (from .config default)
let $retries = config.max_retries # 3 (from .config default)
```

**Null handling.** TOML has no null literal. Keys that are absent from both the `.config` defaults and `lace.config` overrides evaluate to null when accessed in rule bodies. To declare a key whose default is null, omit it from the `[config]` section entirely — rule bodies will receive null via normal null propagation (§5.5).

**No `env:` substitution in defaults.** The `env:VARNAME` and `env:VARNAME:default` resolution mechanism does **not** apply to values in `.config` files. Environment variable substitution is reserved for `lace.config` overrides, where deployment-specific values belong. Defaults in `.config` files are static and portable.

### 2.1 TOML Format Constraint

`.laceext` files **must** be parseable by any TOML 1.0-compliant parser. In particular:

- Inline tables (`{ key = value, … }`) **must not span multiple lines** — TOML 1.0 explicitly forbids this (see [TOML 1.0 §inline-table](https://toml.io/en/v1.0.0#inline-table)).
- Multi-line structured data must use **sub-tables** (`[parent.child]`) or **arrays of tables** (`[[parent.child]]`) instead.

**Rewriting patterns:**

```toml
# ❌ Invalid — multi-line inline table
[result.types.event]
fields = {
  id      = "string",
  payload = "string"
}
```

```toml
# ✅ Valid — sub-table
[result.types.event.fields]
id      = "string"
payload = "string"
```

```toml
# ❌ Invalid — array of multi-line inline tables
[types.notification_val]
one_of = [
  { tag = "template", fields = { name = "string" } },
  { tag = "text",     fields = { value = "string" } }
]
```

```toml
# ✅ Valid — array of sub-tables
[[types.notification_val.one_of]]
tag = "template"
[types.notification_val.one_of.fields]
name = "string"

[[types.notification_val.one_of]]
tag = "text"
[types.notification_val.one_of.fields]
value = "string"
```

Executors using Python's stdlib `tomllib`, Rust's `toml` crate, Go's `BurntSushi/toml`, JavaScript's `@iarna/toml`, and other conformant parsers will reject multi-line inline tables with an "unterminated inline table" or equivalent error. Authors and spec editors must use sub-table form to remain portable.

---

## 3. Schema Additions

Schema additions declare new fields the extension registers on existing objects. When an extension is active, the validator accepts these fields. When inactive, they produce unknown-field warnings.

### 3.1 Registration Targets

| Target key | Where the field appears |
|---|---|
| `scope_options` | The `options {}` block of any scope in `.expect()` / `.check()` |
| `condition_options` | The `options {}` block of any condition in `.assert()` |
| `timeout` | The `timeout {}` call config sub-object |
| `redirects` | The `redirects {}` call config sub-object |
| `security` | The `security {}` call config sub-object |
| `call` | The root call config object |

```toml
[schema.scope_options]
silentOnRepeat = { type = "bool", default = "true" }
notification   = { type = "notification_val" }

[schema.condition_options]
silentOnRepeat = { type = "bool", default = "true" }
notification   = { type = "notification_val" }

[schema.timeout]
notification = { type = "notification_val" }
```

Field definitions:

| Key | Required | Description |
|---|---|---|
| `type` | Yes | Type name — either a built-in type or a name defined in `[types]` |
| `default` | No | Default value as a string. If absent, field defaults to `null`. |
| `required` | No | Bool. If `true`, validator emits error when field is absent and extension is active. Default `false`. |

### 3.2 Type System

**Built-in types:**

| Name | Description |
|---|---|
| `string` | UTF-8 string |
| `int` | Integer |
| `float` | Floating point |
| `bool` | `true` or `false` |
| `null` | Null value |
| `any` | Any type |
| `array<T>` | Array of type T |
| `map<K, V>` | Object with key type K and value type V |
| `string?` | Nullable string (shorthand for `string \| null`) |

**Custom types** defined in `[types]`:

```toml
[types.notification_val]
one_of = [
  { tag = "template", fields = { name = "string" } },
  { tag = "text",     fields = { value = "string" } },
  { tag = "op_map",   fields = { ops = "map<op_key_or_value, notification_val>" } }
]

[types.op_key_or_value]
# Accepts: op literals (lt/lte/eq/neq/gte/gt),
# arbitrary string (treated as eq to actual_value),
# or "default" (fallback when no other key matches)
type = "string"
```

`one_of` declares a tagged union — the value must match exactly one variant. The `tag` field identifies which variant. In the rule language, variants are accessed via their field names after checking the tag.

---

## 4. Result Additions

Extensions declare what they add to the result under `actions.*` or `runVars.*`.

```toml
[result.actions.notifications]
type = "array<notification_event>"

[result.types.notification_event.fields]
callIndex      = "int"
conditionIndex = "int"
trigger         = "string"
scope           = "string?"
notification    = "notification_val"

[result.actions.execution_log]
type = "array<log_entry>"

[result.types.log_entry.fields]
level       = "string"
callIndex   = "int"
message     = "string"
```

These declarations serve two purposes:
1. The validator can type-check `emit` statements in rules against the declared structure
2. Downstream tooling (backends, schema validators) knows what to expect from extensions

> **TOML format constraint.** `.laceext` files must be parseable by any
> TOML 1.0-compliant parser. TOML 1.0 forbids inline tables (`{ … }`) from
> spanning multiple lines. Use sub-tables — e.g.
> `[result.types.foo.fields]` — for any structure that would otherwise
> require multi-line inline syntax. See §2.1 for the full constraint.

---

## 5. Rule Body Language

### 5.1 Formal Grammar

The canonical machine-readable grammar ships alongside this document as
[`laceext.g4`](./laceext.g4) — an ANTLR4 grammar covering everything
below. The ANTLR grammar assumes that the host runtime provides a
pre-lexer (mirroring `lacelang_executor/laceext/dsl_lexer.py`) that
emits synthetic `INDENT` / `DEDENT` / `NEWLINE` tokens around the
base-lexer output; see the file's header comment for the expected
contract. The EBNF below is the human-readable mirror.

```ebnf
rule_body   = statement* ;

statement   = for_stmt
            | when_stmt
            | let_stmt
            | set_stmt
            | emit_stmt
            | exit_stmt
            | fn_call_stmt
            | qualified_call_stmt ;

for_stmt    = "for" binding "in" expr ":" NEWLINE INDENT rule_body DEDENT ;

when_stmt   = "when" expr ":" NEWLINE INDENT rule_body DEDENT
            | "when" expr ;   (* inline form — no body; exits current scope if false *)

let_stmt    = "let" binding "=" expr ;

set_stmt    = "set" binding "=" expr ;   (* function-only — reassigns an existing binding *)

emit_stmt   = "emit" emit_target "<-" "{" emit_field ("," emit_field)* [","] "}" ;

exit_stmt   = "exit" ;   (* exits current rule body — not valid in functions *)

fn_call_stmt = IDENT "(" arg_list? ")" ;   (* function called for side effects only *)

(* Qualified-call statement form: `ext.fn(...)` called at statement
   position for its side effects (typically an `emit` inside the exposed
   function's body). The returned value is discarded. *)
qualified_call_stmt = IDENT "." IDENT "(" arg_list? ")" ;

emit_target = "result" "." IDENT ("." IDENT)* ;

emit_field  = IDENT ":" expr ;

binding     = "$" IDENT ;

fn_body     = (statement | set_stmt)* return_stmt ;

return_stmt = "return" expr ;
(* return is ONLY valid in function bodies.
   Rule bodies use exit_stmt for early termination.
   set_stmt is ONLY valid in function bodies — rule bindings are immutable. *)

(* Expressions.
   Precedence (highest → lowest):
     - primary / access
     - unary `not`
     - arithmetic `*` `/`, then `+` `-`
     - comparisons `eq` `neq` `lt` `lte` `gt` `gte` (non-chaining)
     - logical `and`, then `or`
     - ternary `cond ? a : b` at the outermost level
   The flat rendering below preserves structure for readability. *)
expr        = ternary_expr
            | expr ("eq" | "neq" | "lt" | "lte" | "gt" | "gte") expr
            | expr ("and" | "or") expr
            | "not" expr
            | expr ("+" | "-" | "*" | "/") expr
            | "(" expr ")"
            | access_expr
            | literal
            | object_lit                    (* §5.1 — inline object literal,
                                               used for exposed-function args *)
            | fn_call_expr ;

object_lit  = "{" object_entry ("," object_entry)* [","] "}" | "{}" ;
object_entry = (IDENT | STRING) ":" expr ;

access_expr = base_access (access_op)* ;

base_access = binding          (* $name — local binding or context field *)
            | "result"         (* root result object *)
            | "prev"           (* previous result *)
            | "this"           (* current response — available in scope hooks *)
            | "null"
            | "true" | "false" ;

access_op   = "." IDENT                          (* field access — null-safe *)
            | "?." IDENT                         (* explicit null-safe access *)
            | "[" expr "]"                       (* array index *)
            | "[?" expr "]" ;                    (* array filter — first match or null *)

ternary_expr = expr "?" expr ":" expr ;

fn_call_expr = fn_name "(" arg_list? ")" ;

fn_name      = IDENT                                  (* local: same extension *)
             | IDENT "." IDENT ;                      (* qualified: exposed fn in dep *)

arg_list    = expr ("," expr)* ;

literal     = STRING | INTEGER | FLOAT | "true" | "false" | "null" ;
```

**Indentation:** rule bodies use consistent indentation (2 or 4 spaces). The inline `when expr` form (no body, no colon+indent) immediately exits the current scope if the expression is false — useful for guard chains.

### 5.2 Statements

**`for $binding in expr:`**

Iterates over an array. `expr` must evaluate to an array or null. If null, the loop body is skipped entirely. `$binding` is available within the loop body only.

```
for $call in result.calls:
  for $a in $call.assertions:
    when $a.outcome eq "failed"
    // $call and $a both in scope here
```

**`when expr` (inline guard)**

When `expr` is false or null, the **block of statements following the guard** is skipped and execution continues after the block. When `expr` is true, the block executes. A block runs from the next line up to (but not including) the next **blank line** or the end of the enclosing scope (function body, `for` body, `when` block, or rule body).

Semantically, inline `when X` is sugar for the block form `when X:` with its body consisting of the statements that follow on non-blank lines:

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

The blank line closes the inline-when's block; `STMT_C` runs unconditionally.

Multiple `when` guards chain by nesting — each successive `when` is itself inside the previous guard's block, so all must pass for later statements to execute:

```
when $a.outcome eq "failed"
when $a.options neq null
when not is_null($a.options.notification)
// all three guards passed — this line runs only when all preceding whens are true
```

is equivalent to:

```
when $a.outcome eq "failed":
    when $a.options neq null:
        when not is_null($a.options.notification):
            // all three guards passed
```

This design makes the idiomatic "early-return with guard" pattern work naturally inside functions:

```
when is_null(notif_cfg)
return null

when notif_cfg.tag eq "template" or notif_cfg.tag eq "text"
return notif_cfg

return map_match(notif_cfg.ops, actual, expected, op)
```

The first block returns `null` only when `notif_cfg` is null; the blank line closes that block. The second block returns `notif_cfg` only when its tag is `template` or `text`. Otherwise execution falls through to the final `return`.

**`when expr:` (block form)**

Explicit block form — when `expr` is false or null, the indented block is skipped. Execution continues after the block. Equivalent to the inline form; use whichever reads better.

```
when $call.response neq null:
  let $status = $call.response.status
  // $status only used here
// execution continues here regardless
```

**`let $binding = expr`**

Binds a name to a value within the current scope. Immutable — the same name cannot be rebound in the same scope. A new `for` iteration starts a fresh scope.

**`set $binding = expr`** *(function bodies only)*

Reassigns an existing binding. Walks up the scope chain to find the binding created by a prior `let` and updates it in-place. This is the only way to mutate a binding — and it is restricted to function bodies where the binding has a bounded, deterministic lifetime. Rule body bindings remain immutable.

`set` is useful for accumulation patterns inside `for` loops:

```
let $sum = 0
for $item in items:
  set $sum = $sum + $item.value
return $sum
```

Using `set` on an unbound name (no prior `let` in any enclosing scope) is a runtime error.

**`emit target <- { fields }`**

Appends an object to a result array. `target` must be a path registered in `[result]`. The executor validates that the target is an array type and that all fields conform to the declared type.

**`exit`**

Exits the current rule body immediately. Not valid in function bodies — functions use `return expr`. Useful for early termination after a sequence of guard conditions.

`return` is not a valid statement in rule bodies. Using `return` in a rule body is a parse error.

### 5.3 Expressions

**Field access** uses `.` notation. All field access is null-safe by default — accessing any field on `null` returns `null` rather than throwing. `?.` is an explicit null-safe marker for readability.

```
$call.response.status          // null if response is null
$call.response?.status         // identical — explicit null-safe
```

**Array index** `[n]` returns the element at index n or null if out of bounds.

**Array filter** `[? condition]` returns the first element for which the condition is true, or null. Within the condition, `$` refers to the current element.

```
result.calls[? $.outcome eq "failed"]   // first failed call or null
$call.assertions[? $.scope eq "status"] // first status assertion or null
```

**Ternary** `expr ? expr : expr` — standard conditional expression.

**Boolean operators:** `and`, `or`, `not`. Short-circuit evaluation.

**Equality:** `eq` and `neq`. `null eq null` is `true`. `null eq value` is `false`.

**Comparison:** `lt`, `lte`, `gt`, `gte`. Both operands must be the same comparable type (int, float, or string). Mixed types return `null` (indeterminate). Comparisons do not chain — `a eq b eq c` is a parse error; compose with `(a eq b) and (b eq c)` instead.

**Arithmetic:** `+`, `-`, `*`, `/`. Integer arithmetic when both operands are integers; float otherwise. Division by zero returns `null`.

**String concatenation:** `+` when both operands are strings.

### 5.4 Types and Type Coercion

No implicit type coercion. Operations between incompatible types return `null`.

| Operation | Left | Right | Result |
|---|---|---|---|
| `+` | int | int | int |
| `+` | float | float | float |
| `+` | int | float | float |
| `+` | string | string | string |
| `+` | any other | any | null |
| `lt`,`gt`,`lte`,`gte` | int or float | int or float | bool |
| `lt`,`gt`,`lte`,`gte` | string | string | bool (lexicographic) |
| `lt`,`gt`,`lte`,`gte` | other | any | null |
| `eq`, `neq` | any | any | bool |

### 5.5 Null Propagation

- Any field access on `null` → `null`
- Any array index on `null` → `null`
- Any array filter on `null` → `null`
- Any arithmetic with `null` operand → `null`
- Any ordered comparison with `null` operand → `null`
- `null eq null` → `true`
- `null neq null` → `false`
- `not null` → `true` (null is falsy)
- `for $x in null:` → loop body skipped, no error
- `when null` → exits current scope (null is falsy)

---

## 6. Functions

Functions are defined in the `[functions]` section of the `.laceext` file. They are called from rule bodies and from other functions.

```toml
[functions.resolve_assert_notif]
params = ["notif_cfg", "lhs", "rhs"]
body   = """
when is_null(notif_cfg)
return null

when notif_cfg.tag eq "template" or notif_cfg.tag eq "text"
return notif_cfg

let $rel = compare(lhs, rhs)
when is_null($rel)
return null

return map_get(notif_cfg.ops, $rel)
"""
```

**Function rules:**
- Parameters are bound as `$param_name` (do not write the `$` in the `params` list — the interpreter prefixes it)
- `return expr` exits the function and produces a value
- A function that reaches the end without a `return` returns `null`
- `exit` is not valid in functions — use `return null` for early null exit
- `return` is not valid in rule bodies — use `exit` there
- `set $name = expr` is valid only in function bodies — it reassigns an existing binding (see §5.2). Using `set` in a rule body is a parse error.
- Functions may call other functions defined in the same extension file and may call primitives
- Recursion is not permitted — the call graph must be a DAG
- Functions are pure by default — they cannot `emit` or access `result` directly. Exception: an **exposed function** (§6.1) may `emit` on behalf of its owning extension when invoked from a dependent, because the emit is attributed to the owner.

### 6.1 Exposed Functions

A function declared with `exposed = true` is callable from rule bodies in other extensions that list the declaring extension in their `require`:

```toml
# In laceNotifications.laceext
[functions.pushNotification]
params = ["event"]
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

```toml
# In a dependent extension
[extension]
name    = "notifRelay"
require = ["laceNotifications"]

[[rules.rule]]
name = "relay_on_call"
on   = ["call after laceNotifications"]
body = """
when call.outcome eq "success"
laceNotifications.pushNotification({
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "relay",
  scope:           null,
  notification:    text("...")
})
"""
```

**Rules:**
- The caller references the function via `<extName>.<fnName>(args)` in rule bodies and function bodies. Because extension names are required to be camelCase (§2, `[a-z][A-Za-z0-9]*`), both sides of the dot tokenise as a single IDENT — the parser needs no adjacency heuristic.
- The caller must list the declaring extension in its `[extension].require`. Omitting the require is a runtime error when the qualified call fires (it is not a load-time error — the executor does not walk rule bodies for reference-checking at load).
- Non-exposed functions are NOT callable from dependents — attempting `other-ext.internal_fn(...)` raises `is not an exposed function`.
- The exposed function executes inside the **owning** extension's interpreter. This means:
  - `emit` attributes to the owner — any `result.actions.*` entries belong to the owner's namespace.
  - `result.runVars` emits from the exposed function use the owner's prefix, not the caller's.
  - The owner's `require_view` is in scope, not the caller's.
  - Primitives, tag constructors, and other internal functions resolve against the owner.
- Recursion and cycles remain forbidden — `extA.foo` cannot call `extB.bar` that calls back into `extA.foo`.
- Exposed functions still accept and return values like any other function.

---

## 7. Primitives

Provided by the executor's rule interpreter. Available in all rule bodies and functions. All implementations provide these identically.

### `compare(a, b) → string | null`

Returns the op key describing the actual relationship between `a` and `b`.

| Condition | Returns |
|---|---|
| `a lt b` | `"lt"` |
| `a lte b` | `"lte"` |
| `a eq b` | `"eq"` |
| `a neq b` (and not ordered) | `"neq"` |
| `a gte b` | `"gte"` |
| `a gt b` | `"gt"` |
| Either operand is `null` | `null` |
| Operands are incomparable types | `null` |

For numeric types, all six ordered relationships are possible. For strings, all six ordered relationships are possible (lexicographic). For booleans, only `eq` and `neq` are meaningful.

### `map_get(map, key) → any | null`

Looks up `key` in `map`. Falls back to `map["default"]` if `key` is absent. Returns `null` if neither `key` nor `"default"` is present. Returns `null` if `map` is null.

```
map_get({ "lt": "a", "default": "b" }, "gt")  // → "b"
map_get({ "lt": "a" }, "gt")                  // → null
map_get({ "eq": "a" }, "eq")                  // → "a"
```

### `map_match(map, actual, expected, op) → any | null`

Resolves the best matching key in a notification-style map for a validation failure. Tries the following in order, returning the first match:

1. The string representation of `actual` as a key (eq comparison — matches literal value keys like `"404"`)
2. The result of `compare(actual, expected)` as a key (op key match)
3. `"default"` as a key

Returns `null` if no key matches. Returns `null` if `map` is null.

```
map_match({"404": t1, "gte": t2, "default": t3}, 404, 200, "eq")
// → t1  (actual "404" matches literal key)

map_match({"gte": t2, "default": t3}, 1200, 500, "lt")
// compare(1200, 500) = "gt" — no "gt" key
// → t3  (default)

map_match({"lt": t4}, 100, 500, "lt")
// compare(100, 500) = "lt" — matches "lt"
// → t4
```

### `is_null(v) → bool`

Returns `true` if `v` is `null`, `false` otherwise.

### `type_of(v) → string`

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

### `to_string(v) → string`

Converts any value to its string representation. Null returns `"null"`, booleans return `"true"` / `"false"`, numbers return their decimal form, strings return themselves.

### `replace(str, pattern, replacement) → string | null`

Returns a copy of `str` with all occurrences of `pattern` replaced by `replacement`. If `str` or `pattern` is null, returns `str` unchanged. The `replacement` value is converted to a string via `to_string()` before substitution.

```
replace("hello $name", "$name", "world")    // → "hello world"
replace("x=$val", "$val", 42)               // → "x=42"
replace(null, "a", "b")                     // → null
```

---

## 8. Hook Points

### 8.1 Hook Execution Model

Two variants exist for each block type: `on before {block}` (fires before the block executes) and `on {block}` (fires after the block executes).

`on before` hooks have no outcome-related fields — the block has not run yet. They are provided for future extensions that may need to inspect or log pre-execution state. No current built-in extension uses them. They must be implemented by all compatible executors.

If an extension emits to a result path that is not yet initialised, the executor initialises it as an empty array before appending.

#### 8.1.1 Rule Ordering at a Hook

Extensions **must not** rely on `lace.config` load order for determinism. At every hook fire, the executor resolves a partial order over rules using explicit qualifiers attached to each `on <hook>` entry:

```toml
[[rules.rule]]
name = "my-rule"
# simple case — fires at `check`, ordered against any `require`d ext that
# also has rules on this hook.
on   = ["check"]

# explicit — fire AFTER every `laceNotifications` rule at `check`
on   = ["check after laceNotifications"]

# reverse — fire BEFORE every `laceMetrics` rule at `check`
on   = ["check before laceMetrics"]

# chain — both at once; and a different shape on another hook
on   = [
  "check after laceNotifications before laceMetrics",
  "assert after laceNotifications"
]
```

Grammar of each entry:

```
on_entry = <hook> ( " " ordering )*
ordering = ("after" | "before") " " <extension-name>
```

Resolution algorithm (per hook, per run):

1. **Gather** — every `[[rules.rule]]` that registers for this hook contributes its `on`-entry constraints.
2. **Expand defaults** — for each rule that belongs to extension B with `require = [A₁, A₂, …]`, an implicit `after Aᵢ` is appended **only when** Aᵢ has at least one rule on this hook. Explicit constraints on the same pair override the default.
3. **Name-resolve** — every name in `after X` / `before X` must match a loaded extension; otherwise startup error. Resolution is against the full load set, not the `require` list (you can order against a peer you don't consume variables from).
4. **Silent-drop unfulfillable constraints** — if a rule has an explicit `after X` or `before X` and X contributes zero rules to this hook, the rule is silently removed from this hook's run set. This can cascade: rules ordered against the dropped rule re-evaluate and may themselves drop.
5. **Topo-sort** — the surviving rules form a DAG (edges from `after` targets to the rule, from the rule to `before` targets). A cycle is a startup error reporting the edge chain.
6. **Execute** — rules run in a topo-consistent order. Ties (rules with no ordering relation to each other) break by declaration order within a file, then by extension name alphabetically. Within a single extension, rules always keep their declaration order unless an explicit constraint says otherwise.

Notes:
- Ordering is **per hook**. A rule can be `after X` at one hook and `before X` at another.
- A rule with no ordering constraints and no `require` entries runs in the free-order position — the executor is free to place it anywhere consistent with other rules' constraints.
- A purely declarative extension (no `[[rules.rule]]`) contributes no edges to any hook; `require`-ing it is valid (it acts as a schema-only contract) and adds no scheduling pressure.

### 8.2 `on before script` / `on script`

Fires once per run at the outer script boundary. `on before script` fires after extensions finish loading but before any call is issued. `on script` fires after every call record is finalised and just before the result is emitted — it is the last extension hook point in a run.

These hooks are useful for setup/teardown that is per-run rather than per-call: opening a connection pool, seeding a shared variable, emitting a run-level event, summarising results, etc.

**`on before script` context:**

| Name | Type | Description |
|---|---|---|
| `script.callCount` | int | Total number of calls declared in the source |
| `script.startedAt` | string | Timestamp at which the run began (ISO 8601 UTC) |
| `prev` | object \| null | Previous result if `--prev-results` was provided |

**`on script` context** (adds the full result):

| Name | Type | Description |
|---|---|---|
| `script.callCount` | int | |
| `script.startedAt` | string | |
| `script.endedAt` | string | Timestamp at which the last call completed |
| `result.outcome` | string | `"success"` \| `"failure"` \| `"timeout"` |
| `result.calls` | array | The finalised call records, in script order |
| `result.runVars` | object | Final `runVars` map |
| `result.actions` | object | Actions accumulated during the run (including any prior extension emits) |
| `prev` | object \| null | |

`on script` MUST fire even when the run hard-failed early or was cut short by `.wait()` being skipped. Extensions that need to flush state should do so here, not from an extension-registered `on call` rule on the last call (which is fragile when cascading failures skip later calls).

Extensions writing to `result.actions.*` or `result.runVars.*` from `on script` rules follow the same namespacing and emit rules as any other hook (§9, §10).

### 8.3 `on before call` / `on call`

Fires before/after all chain methods on a single HTTP call complete.

**`on before call` context:**

| Name | Type | Description |
|---|---|---|
| `call.index` | int | Zero-based call index |
| `call.request` | object | Resolved request: `url`, `method`, `headers`, `bodyPath` |
| `call.config` | object | Resolved call config including all extension fields |
| `prev` | object \| null | Previous result |

**`on call` context** (adds outcome fields):

| Name | Type | Description |
|---|---|---|
| `call.index` | int | |
| `call.outcome` | string | `"success"` \| `"failure"` \| `"timeout"` \| `"skipped"` |
| `call.response` | object \| null | Full response or null |
| `call.assertions` | array | All assertion records from this call |
| `call.config` | object | Resolved call config |
| `prev` | object \| null | |

### 8.4 `on before expect` / `on expect`

Fires before/after each scope in `.expect()` is evaluated.

**`on before expect` context:**

| Name | Type | Description |
|---|---|---|
| `scope.name` | string | Scope name e.g. `"status"`, `"totalDelayMs"` |
| `scope.value` | any | Expected value |
| `scope.op` | string | Comparison op |
| `scope.options` | object \| null | The `options {}` object from source |
| `call.index` | int | |
| `this` | object | Current response |
| `prev` | object \| null | |

**`on expect` context** (adds outcome and actual):

All `on before expect` fields, plus:

| Name | Type | Description |
|---|---|---|
| `scope.actual` | any | Actual value observed |
| `scope.outcome` | string | `"passed"` \| `"failed"` \| `"indeterminate"` |

### 8.5 `on before check` / `on check`

Identical context structure to `on before expect` / `on expect`. The only difference is that `on check` hooks fire for `.check()` scopes.

### 8.6 `on before assert` / `on assert`

Fires before/after each condition in `.assert()` is evaluated.

**`on before assert` context:**

| Name | Type | Description |
|---|---|---|
| `condition.index` | int | Index within the assert array |
| `condition.kind` | string | `"expect"` \| `"check"` |
| `condition.expression` | string | Expression as written in source |
| `condition.options` | object \| null | The `options {}` object |
| `call.index` | int | |
| `this` | object | Current response |
| `prev` | object \| null | |

**`on assert` context** (adds outcome and operands):

All `on before assert` fields, plus:

| Name | Type | Description |
|---|---|---|
| `condition.actualLhs` | any | Resolved left operand |
| `condition.actualRhs` | any | Resolved right operand |
| `condition.outcome` | string | `"passed"` \| `"failed"` \| `"indeterminate"` |

### 8.7 `on before store` / `on store`

Fires before/after each entry in `.store()` is processed.

**`on before store` context:**

| Name | Type | Description |
|---|---|---|
| `entry.key` | string | Store key as written (includes `$`/`$$` prefix if present) |
| `entry.value` | any | Value to be written |
| `entry.scope` | string | `"run"` (for `$$` keys) \| `"writeback"` (for `$` and plain keys) |
| `call.index` | int | |
| `this` | object | Current response |
| `prev` | object \| null | |

**`on store` context** (adds written flag):

All `on before store` fields, plus:

| Name | Type | Description |
|---|---|---|
| `entry.written` | bool | Whether the write succeeded (false if skipped due to soft fail) |

---

## 9. Extension Variables

Extensions may persist state across hook invocations within a single run by writing to `result.runVars`. All keys must be prefixed with the extension name followed by a dot:

```
emit result.runVars <- {
  "laceNotifications.suppressedCount": $suppressedCount
}
```

**Rules:**
- Extension name is taken from `[extension].name` in the `.laceext` file
- Any `emit result.runVars <- { key: value }` where `key` does not start with `{extension_name}.` is a runtime error — the emit is rejected and a warning is recorded
- Extension variables are not readable from `.lace` scripts — they are for extension-internal state and backend consumption only
- Extension variables appear in `runVars` alongside `$$var` entries from the script. The `{extension_name}.` prefix prevents collisions with script author keys (which are always bare identifiers).
- Values may be any JSON-serialisable shape — scalar, object, or array (§4.6).
- An extension **cannot read back its own runVars**. If it needs per-run state, it must accumulate it on a `let $name` binding inside the rule or via a dedicated array under `result.actions.{key}` and read from there.

### 9.1 Reading another extension's variables

An extension **B** may read the runVars emitted by extension **A** during the current run iff B declares A in its `require` list (§2.2). Reads go through the `require` base in the rule body language:

```toml
# Extension B's .laceext
[extension]
name    = "rateLimitNotifications"
version = "1.0.0"
require = ["laceNotifications"]
```

```
# Inside a B rule body: read A's live runVars
let $suppressed = require["laceNotifications"]["laceNotifications.suppressedCount"]
when is_null($suppressed)
let $suppressed = 0
```

| Access | Resolves to |
|---|---|
| `require["<dep-name>"]` | Map of that dep's current runVars, keyed by the full emitted key (e.g. `"laceNotifications.suppressedCount"`). Returns `null` if the dep has emitted nothing yet, or if `<dep-name>` isn't in `require`. |
| `require["<dep-name>"]["<key>"]` | The value, or `null` if the dep has not emitted that key yet. |
| `require` with no accessor | Returns a map keyed by dep-name. Useful for iterating: `for $k in …`. |

**Reads are never-fails** — accessing a non-required extension, or a key that hasn't been emitted yet, returns `null`. The DSL's null-propagation rules (§5.5) apply.

**Reads reflect current run state**: B sees whatever A has emitted at the moment B's rule evaluates. If A's hooks haven't yet fired (or haven't yet emitted the key B is asking for), the read is `null`. Authors must not assume a specific hook-execution order between A and B based on load order.

---

## 10. Emit Targets

`emit` may only target paths declared in the extension's `[result]` section, plus `result.runVars` (always available to all extensions under their namespace).

| Target | Type | Behaviour |
|---|---|---|
| `result.actions.{key}` | array | Appends to the array. Initialised to `[]` if not yet present. |
| `result.runVars` | object | Merges key-value pair into the runVars map. Key must be prefixed with extension name. |

Attempting to emit to any other path is a runtime error. The emit is rejected, a warning is recorded in the call's `warnings` array, and execution continues.

Emitting to `result.calls`, `result.outcome`, `result.startedAt`, `result.endedAt`, or any other core result field is never permitted.

---

## 11. Configuration

Each extension may declare a configuration section in `lace.config` under `[extensions.{name}]`. Extension config is accessible in rule bodies and functions as `config`:

```
let $prev_path = config.prev_results
```

Config values are strings by default. The extension's schema additions may declare typed config fields (not yet specified — extension config typing is a v2 concern). For now, all config values are strings or null.

`env:VARNAME` and `env:VARNAME:default` resolution applies to extension config values in `lace.config` — resolved at startup before any rule runs. It does **not** apply to `.config` file defaults (see §2.3).

### 11.1 Merge Order

When both a `.config` file (§2.3) and `lace.config` overrides exist for an extension, values are merged as follows:

1. **Base layer: extension `.config` defaults.** All key-value pairs from the `[config]` section of the extension's `.config` file form the base configuration.
2. **Override layer: `lace.config [extensions.{name}]`.** Any key present in the `lace.config` section for this extension overwrites the corresponding base value. Keys present in the `.config` defaults but **absent** from `lace.config` retain their default values — they are **not** nullified by omission.
3. **Final `config` namespace.** The merged result is exposed to rule bodies and functions as `config.{key}`.

**Reserved key.** The `laceext` key in `[extensions.{name}]` is reserved for structural use (it specifies the path to the `.laceext` file). It is stripped before the merge and is never visible in the `config` namespace.

**Unloaded extensions.** Config values in `lace.config` for extensions that are not loaded (not listed or failed to load) are silently dropped. No error is raised.

**Example.** Given a `.config` file:

```toml
[extension]
name    = "laceNotifications"
version = "1.0.0"

[config]
channel      = "general"
max_retries  = 3
include_body = true
```

And a `lace.config` override:

```toml
[extensions.laceNotifications]
laceext      = "builtin:laceNotifications"
channel      = "env:NOTIFY_CHANNEL:alerts"
```

The effective config is:

| Key            | Value                              | Source           |
|----------------|------------------------------------|------------------|
| `channel`      | resolved value of `env:NOTIFY_CHANNEL` (or `"alerts"`) | `lace.config` override |
| `max_retries`  | `3`                                | `.config` default |
| `include_body` | `true`                             | `.config` default |

---

## 12. Reference Extension: `laceNotifications`

The `laceNotifications` extension is the reference implementation of the extension system. It is bundled with all executor distributions as `builtin:laceNotifications` and serves as the canonical example for extension authors.

The full extension source, configuration defaults, notification type documentation, and conformance vectors live in `extensions/default/laceNotifications/`:

| File | Purpose |
|---|---|
| `laceNotifications.laceext` | Extension definition: schema additions, custom types, functions, rules |
| `laceNotifications.config` | Default config values (§2.3) |
| `README.md` | Notification type system (`text`, `template`, `structured`, `op_map`), backend contract, aggregation guidance |
| `vectors/` | Extension-specific conformance vectors |

**Key capabilities:**

- Registers `notification` and `silentOnRepeat` options on scopes, conditions, and timeouts
- Declares the `notification_val` tagged union type with four variants: `text`, `template`, `structured`, `op_map`
- Emits `notification_event` entries into `result.actions.notifications` on assertion failures and timeouts
- Exposes `pushNotification(event)` for peer extensions to inject notifications via qualified calls (§6.1)
- Fires default `structured()` notifications when no custom notification option is set, giving the backend machine-readable failure data

See `extensions/default/laceNotifications/README.md` for the full notification type documentation and backend integration guide.

---

## 12.1 Reference Extension: `laceBaseline`

The `laceBaseline` extension monitors rolling averages of HTTP response timing metrics across probe runs and detects spikes. It depends on `laceNotifications` (calls `pushNotification()`) and serves as the reference example of extension composition via `require`.

The full extension source, configuration, and vectors live in `extensions/default/laceBaseline/`:

| File | Purpose |
|---|---|
| `laceBaseline.laceext` | Extension definition: result actions, spike detection, stats accumulation |
| `laceBaseline.config` | Default config (`min_entries`, `spike_multiplier`, `spike_action`) |
| `README.md` | Stats model, spike detection algorithm, config reference |
| `vectors/` | Extension-specific conformance vectors |

**Key capabilities:**

- Tracks rolling averages of 7 response timing metrics: `responseTimeMs`, `dnsMs`, `connectMs`, `tlsMs`, `ttfbMs`, `transferMs`, `sizeBytes`
- Detects spikes when a metric exceeds `average * spike_multiplier` (configurable, default 3.0)
- Emits `baseline_spike_event` entries into `result.actions.baseline` on spike detection
- Pushes `structured()` notifications via `laceNotifications.pushNotification()` for each spike
- Accumulates stats across runs via `result.runVars["laceBaseline.stats"]` — the backend persists the result and passes it as `prev` on the next run
- Exposes `check_spike(stats, metric_name, actual, call_index, multiplier)` for peer extensions to perform custom baseline checks
- Uses the `set` statement (§5.2) in its `accumulate_stats` function for `for`-loop accumulation — demonstrating the mutable-binding pattern for data aggregation in the DSL

See `extensions/default/laceBaseline/README.md` for the full stats model and configuration reference.

---

## 13. Executor Compatibility Checklist

Moved to **[checklist-extensions.md](./checklist-extensions.md)** for maintainability. The checklist covers extension loading, schema additions, rule body language, expressions, primitives, functions, hook points, extension variables, configuration, and result conformance.

---

*End of Lace extension system v0.9.0*
