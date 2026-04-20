# Extension Notes

Companion to `schemas/laceext.json` and `lace-extensions.md`. Documents intentional choices in the extension file format and the bundled reference extensions.

The `.laceext` file format is canonically defined by `schemas/laceext.json`.

---

## Implementation Conventions

### TOML Table Style

TOML 1.0 requires inline tables to fit on a single line. All `.laceext` files use standard TOML sub-tables (`[parent.child]`) and arrays of sub-tables (`[[parent.child]]`) rather than multi-line inline tables. The spec (section 2.1) codifies this constraint explicitly.

### Inline `when` Scope

Inline `when X` is syntactic sugar for the block form `when X:` whose body comprises the statements that follow on non-blank lines. Blank lines close the block. This makes the early-return-with-guard idiom work as written and keeps the chained-guard example in section 5.2 valid.

### Rule Body and Function Body Opacity at the Schema Level

`schemas/laceext.json` treats rule bodies and function bodies as opaque strings. Their content is governed by the rule body language grammar defined in `lace-extensions.md` section 5, which is parsed by the executor's extension processor -- **not** by the `.laceext` schema validator.

**Why**: rule bodies are a small embedded language (with `for`, `when`, `let`, `set`, `emit`, `exit`, function calls, expressions). Encoding it in JSON Schema is impractical and would duplicate validation logic. The extension processor parses these strings using its own grammar.

**Implication**: a `.laceext` file can pass schema validation but contain syntactically invalid rule bodies. The extension processor rejects these at extension load time, before any rule executes.

### Extension Namespace Prefix

`lace-extensions.md` section 9 requires extension `runVars` keys to be prefixed with the extension name. The schema does not enforce this -- it is a runtime concern (the extension processor rejects emits with wrong-prefix keys per error code `EXT_RUN_VAR_NAMESPACE`).

The extension's `[extension].name` field is constrained to `^[a-z][A-Za-z0-9]*$` in the schema (camelCase, no hyphens or underscores). This pattern is the source of truth for valid extension names.

### Hook Name Enumeration

`schemas/laceext.json` enumerates the twelve valid hook names as the closed enum on `RuleDef.on`. Adding a hook point is a breaking change to both the spec and the schema. The extension processor uses this same enum to resolve hook registrations.

---

## Testing

The bundled `laceNotifications.laceext` and `laceBaseline.laceext` files serve as test vectors for the laceext schema. Any change to the schema must keep these files valid; any change to the extensions must remain schema-conformant. The `make verify` target in `lacelang/specs/Makefile` runs this check.
