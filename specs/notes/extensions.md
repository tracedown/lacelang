# Extensions Notes

> Companion to `schemas/laceext.json` and `lace-extensions.md`.
> Documents intentional choices in the extension file format and
> the bundled reference extensions.

The `.laceext` file format is canonically defined by `schemas/laceext.json`.

---

## Implementation conventions

### TOML table style

TOML 1.0 requires inline tables to fit on a single line. The bundled
`.laceext` files keep every inline table on one line; `one_of` lists are
multi-line arrays of single-line inline tables, which is valid TOML 1.0.
Structures that would need a multi-line inline table use sub-tables
(`[parent.child]`) instead. The spec (§2.1) codifies this constraint.

### Inline `when` scope

Inline `when X` is syntactic sugar for the block form `when X:` whose
body comprises the statements that follow on non-blank lines. Blank lines
close the block. This makes the early-return-with-guard idiom work as
written and keeps the chained-guard example in §5.2 valid.

### Rule body and function body opacity at the schema level

`schemas/laceext.json` treats rule bodies and function bodies as opaque
strings. Their content is governed by the rule body language grammar
defined in `lace-extensions.md §5`, which is parsed by the executor's
extension processor — **not** by the .laceext schema validator.

**Why**: rule bodies are a small embedded language (with `for`, `when`,
`let`, `set`, `emit`, `exit`, function calls, expressions). Encoding it
in JSON Schema is impractical and would duplicate validation logic. The
extension processor parses these strings using its own grammar.

**Implication**: a `.laceext` file can pass schema validation but contain
syntactically invalid rule bodies. The extension processor rejects these
at extension load time, before any rule executes.

### Function parameters are bare identifiers

`[functions.X].params` names are bound as bare identifiers in the body
(`notif_cfg`, not `$notif_cfg`), exactly like hook context objects.
`$`-prefixed names are reserved for `let` / `for` bindings
(lace-extensions.md §6).

### Extension namespace prefix

`lace-extensions.md §9` requires extension `runVars` keys to be prefixed
with the extension name. The schema doesn't enforce this — it's a
runtime concern (the extension processor rejects emits with wrong-prefix
keys per error code `EXT_RUN_VAR_NAMESPACE`).

The extension's `[extension].name` field is constrained to
`^[a-z][A-Za-z0-9]*$` in the schema (camelCase, no hyphens or
underscores). This pattern is the source of truth for valid extension
names.

### Hook name enumeration

`schemas/laceext.json` enumerates the twelve valid hook names as the closed
enum on `RuleDef.on`. Adding a hook point is a breaking change to both
the spec and the schema. The extension processor uses this same enum to
resolve hook registrations.

---

## Testing

Every `.laceext` under `extensions/` — the bundled `laceNotifications`,
`laceBaseline` and `laceEmitRecovery` plus the `extensions/test/` set —
serves as a test vector for the laceext schema. Any change to the schema
must keep these files valid; any change to the extensions must remain
schema-conformant. The `make verify` target in `lacelang/specs/Makefile`
runs this check.
