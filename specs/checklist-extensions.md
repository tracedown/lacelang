# Extension System Compatibility Checklist

> Spec version: 0.9.0
> Companion to: [lace-extensions.md](./lace-extensions.md)

An executor implementation is considered **Lace Extension Compatible** when it satisfies all items in this checklist. Partial compatibility must be documented — an executor may declare which sections it supports.

---

## 1. Extension Loading

- [ ] Reads `.laceext` files listed in `lace.config` at startup
- [ ] Fails with a clear error if a listed extension file is not found
- [ ] Loads extensions in the order listed in config
- [ ] Parses the TOML structure and validates it against the `.laceext` schema
- [ ] Rejects `.laceext` files with unknown top-level sections (warning, not error)

## 2. Schema Additions

- [ ] Registers extension fields with the validator at the declared targets
- [ ] Accepts registered fields without error when extension is active
- [ ] Emits unknown-field warnings (not errors) for extension fields when extension is inactive
- [ ] Does not emit errors for unknown fields in `options {}` blocks regardless of extension state

## 3. Rule Body Language

- [ ] Parses rule bodies and function bodies using the grammar in lace-extensions.md §5.1
- [ ] Executes `for` iteration with named binding; skips body when collection is null
- [ ] Executes inline `when expr` as a guard that exits current scope on false/null
- [ ] Executes block `when expr:` as a conditional block skip
- [ ] Executes `let $name = expr` as an immutable local binding within current scope
- [ ] Rejects rebinding the same `$name` in the same scope
- [ ] Executes `set $name = expr` as reassignment of an existing binding (walks up scope chain)
- [ ] Rejects `set` in rule bodies — parse error
- [ ] Rejects `set` on an unbound name — runtime error
- [ ] Executes `emit target <- { fields }` as an append to the target array
- [ ] Rejects `emit` to paths not declared in `[result]` or `result.runVars`
- [ ] Executes `exit` as immediate exit from rule body
- [ ] Rejects `exit` inside function bodies — parse error
- [ ] Rejects `return` inside rule bodies — parse error
- [ ] Executes `return expr` inside function bodies only

## 4. Expressions

- [ ] Evaluates all arithmetic operators with correct type rules (lace-extensions.md §5.3, §5.4)
- [ ] Evaluates all comparison operators with correct type rules
- [ ] Evaluates `and`, `or`, `not` with short-circuit evaluation
- [ ] Null propagation: field access, array index, array filter on null returns null (lace-extensions.md §5.5)
- [ ] `null eq null` evaluates to `true`
- [ ] `not null` evaluates to `true`
- [ ] Array filter `[? condition]` returns first matching element or null
- [ ] Ternary `a ? b : c` evaluates correctly; falsy `a` (false or null) takes the `c` branch
- [ ] String concatenation with `+`
- [ ] No implicit type coercion — incompatible types return null

## 5. Primitives

- [ ] `compare(a, b)` returns correct op key for all comparable type pairs; null for null/incomparable inputs
- [ ] `map_get(map, key)` returns correct value; falls back to `"default"`; null for absent/null
- [ ] `map_match(map, actual, expected, op)` tries literal key, then op key, then `"default"` in order
- [ ] `is_null(v)` returns bool correctly
- [ ] `type_of(v)` returns correct type string for all value types
- [ ] `to_string(v)` returns string representation for all value types
- [ ] `replace(str, pattern, replacement)` performs string substitution; null-safe

## 6. Functions

- [ ] Parses function definitions from `[functions]` section
- [ ] Calls extension functions by name from rule bodies and other functions
- [ ] Parameters bound as `$param_name`
- [ ] `return expr` exits function and produces value
- [ ] Function reaching end without `return` returns null
- [ ] Rejects recursive calls (cycle detection at load time)
- [ ] Functions cannot `emit` or access `result` directly

## 7. Hook Points

- [ ] `on before call` fires before any chain method on a call executes
- [ ] `on call` fires after all chain methods on a call complete
- [ ] `on before expect` fires before each `.expect()` scope is evaluated
- [ ] `on expect` fires after each `.expect()` scope is evaluated (once per scope, not once per method)
- [ ] `on before check` / `on check` — same as expect for `.check()`
- [ ] `on before assert` / `on assert` fires per condition in `.assert()`
- [ ] `on before store` / `on store` fires per entry in `.store()`
- [ ] Rules from multiple extensions at the same hook are ordered per lace-extensions.md §8.1.1 — by explicit `after` / `before` qualifiers and implicit `after` edges from `require`, topo-sorted with ties broken deterministically (declaration order within a file, then extension name alphabetically). Executors **must not** rely on `lace.config` load order for determinism.
- [ ] Rules within a single extension run in declaration order (unless explicit ordering qualifiers say otherwise)

## 8. Extension Variables

- [ ] `emit result.runVars <- { key: value }` merges into `runVars`
- [ ] Rejects keys not prefixed with `{extension_name}.` — records warning, continues
- [ ] Extension variables appear in final `runVars` map
- [ ] Extension variables are not injected into the script's `$var` namespace
- [ ] Values may be any JSON-serialisable shape (scalar, object, or array)

## 9. Configuration

- [ ] Loads sibling `{extName}.config` file when present (lace-extensions.md §2.3); absence is not an error
- [ ] Reads `[extensions.{name}]` config section from `lace.config`
- [ ] Merges `.config` defaults with `lace.config` overrides per lace-extensions.md §11.1 (overrides win, defaults preserved for absent keys)
- [ ] Provides merged `config` object to rule bodies containing the extension's config values
- [ ] Resolves `env:VARNAME` and `env:VARNAME:default` in `lace.config` values at startup (not in `.config` defaults)

## 10. Result Conformance

- [ ] `emit result.actions.{key}` initialises to `[]` if not yet present
- [ ] Final result contains all emitted extension fields under `actions`
- [ ] Extension variables appear in `runVars` with correct prefixes
- [ ] Core result fields (`calls`, `outcome`, `runVars` script entries, `startedAt`, `endedAt`) are never mutated by extension rules
