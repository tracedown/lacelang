# Extensions Overview

Extensions add functionality to Lace without modifying the core language. An extension is a **`.laceext` file** -- a TOML document containing schema additions, result declarations, functions, and rules. The executor's built-in extension processor interprets `.laceext` files using the rule body language defined in this section.

## Design principles

- **Declarative.** No imperative code runs from an extension -- only the rule language.
- **Portable.** The rule language is identical across all executor implementations (Python, JavaScript, Kotlin). An extension written once runs the same everywhere.
- **Isolated.** Extensions may only write to their own namespace in `runVars`. They cannot modify `calls`, `outcome`, or other extensions' data.
- **Optional.** The core executor never depends on any specific extension. The bundled extensions -- `laceNotifications`, `laceEmitRecovery` and `laceBaseline` -- ship as `.laceext` files but are inactive until activated in `lace.config` (`[executor] extensions`) or with `--enable-extension`.

## File structure

A `.laceext` file has six top-level sections:

```toml
[extension]
name    = "myExtension"      # camelCase identifier
version = "1.0.0"
require = []                  # optional dependencies

[schema]
# Schema additions -- new fields on existing objects

[types]
# Custom types used by schema and result additions

[result]
# Result additions -- new entries in the run result

[functions]
# Reusable functions called from rules

[rules]
# Rules that fire at hook points during execution
```

All sections except `[extension]` are optional. An extension with schema additions but no rules is a pure schema extension. An extension with rules but no schema additions is a pure post-processing extension.

**Extension name constraint.** The `name` must match `[a-z][A-Za-z0-9]*` -- lowercase-leading camelCase with no hyphens or underscores. This keeps qualified function calls like `extName.fnName()` unambiguous.

## Loading

Extensions are activated by listing them in `lace.config` under `[executor] extensions`:

```toml
[executor]
extensions = ["laceNotifications", "laceBaseline", "myCustomExt"]

# Optional: configure an extension, or point a custom one at its file
[extensions.myCustomExt]
laceext = "./extensions/myCustomExt/myCustomExt.laceext"
```

Or activate one for a single run with `--enable-extension NAME` (repeatable):

```bash
lacelang-executor run script.lace --enable-extension laceNotifications
```

Activation and configuration are separate. An `[extensions.<name>]` table only configures an extension (bundled extensions default to `laceext = "builtin:<name>"`); on its own it activates nothing, and a table for an extension that is not activated is silently ignored.

The executor loads the `.laceext` files of every activated extension at startup. If a file is not found, startup fails with an error. Load order carries no ordering semantics -- the order in which rules run at a hook is resolved from `require` and explicit `after` / `before` qualifiers (see [Rule ordering](hooks.md#rule-ordering)).

## Dependencies

An extension declares dependencies with `require`:

```toml
[extension]
name    = "myExtension"
version = "1.0.0"
require = ["laceNotifications"]
```

What `require` gives you:

- **Presence check** -- every name in `require` must be a loaded extension. If any is missing, startup fails with a clear error.
- **Read access** -- the depending extension can read `result.runVars` entries emitted by required extensions via `require["depName"]["depName.key"]`.
- **Implicit ordering** -- at hooks where a required extension has rules, the depending extension's rules run after them by default.

What `require` does not do:

- No write access to the dependency's `runVars`.
- No transitive read -- if A requires B and B requires C, A does not automatically see C's variables. A must list C in its own `require`.
- No version negotiation (out of scope for v1).

## Companion config file

An extension may ship a `.config` file alongside the `.laceext` file that declares default configuration values. For `myExtension.laceext`, the config file is `myExtension.config` in the same directory.

```toml
[extension]
name    = "myExtension"
version = "1.0.0"

[config]
threshold = 100
channel   = "general"
```

The `[extension]` header must match the `.laceext` file. Config values are accessible in rules and functions as `config.key`. See [Variables & Config](variables-and-config.md) for the full merge order with `lace.config` overrides.

## Sub-pages

| Page | What it covers |
|------|----------------|
| [Schema Additions](schema-additions.md) | Registering new fields on scopes, conditions, timeouts, and calls |
| [Result Additions](result-additions.md) | Declaring result arrays and custom types |
| [Rule Language](rule-language.md) | Statement reference: `for`, `when`, `let`, `set`, `emit`, `exit`, `return` |
| [Expressions](expressions.md) | Field access, operators, ternary, null propagation |
| [Functions](functions.md) | Defining and exposing functions, primitives reference |
| [Hook Points](hooks.md) | All 12 hooks, context objects, rule ordering |
| [Variables & Config](variables-and-config.md) | Config files, merge order, `runVars` namespacing |
| [Built-in Extensions](built-in/index.md) | laceNotifications, laceEmitRecovery and laceBaseline reference |
