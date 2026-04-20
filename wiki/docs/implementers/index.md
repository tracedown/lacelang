# For Implementers

This section covers what you need to build a conformant Lace executor or validator.

## What is a Lace implementation?

A Lace implementation consists of two packages:

1. **Validator** -- lexer, parser, semantic checks, canonical error codes. Zero network dependencies. Exposes `parse` and `validate` CLI subcommands.
2. **Executor** -- HTTP runtime, assertion evaluation, cookie jars, extension dispatch, body storage. Depends on the validator. Exposes `parse`, `validate`, and `run` CLI subcommands.

See [Packaging](packaging.md) for the separation rules and rationale.

## Steps to Build a Conformant Executor

### 1. Implement the Parser

- Parse `.lace` source text against the formal grammar (`lacelang.g4` is the authoritative ANTLR4 grammar).
- Produce an internal AST matching the [AST schema](../reference/ast-schema.md).
- See [Grammar Notes](notes/grammar.md) for intentional divergences between the ANTLR4 grammar and the spec EBNF.

### 2. Implement the Validator

- Enforce all validation rules from the spec (section 12).
- Emit canonical [error codes](../reference/error-codes.md) -- cross-implementation conformance depends on matching codes.
- Report all errors before execution (do not stop at the first error).
- Distinguish errors (block activation) from warnings (allow execution with notice).

### 3. Implement the Executor

- Execute calls sequentially, evaluate chain methods, apply the failure cascade.
- Produce a [ProbeResult](../reference/result-schema.md) matching the result schema.
- Implement all core features: variables, null semantics, cookie jars, body storage, `prev` access.
- Implement the [extension interface](../reference/primitives.md) (hook dispatch, schema additions, rule body language) for full conformance -- or declare `omit: extensions` for partial conformance.

### 4. Pass the Testkit

Run the conformance suite:

```bash
lace-conformance -c "<your-executor-command>"
```

The testkit ships a bundled HTTP/HTTPS test server and test vectors covering every behavioural item in the core checklist. Exit code 0 means conformant.

See [Conformance Levels](conformance-levels.md) for partial conformance options.

## Key Resources

- [Core Checklist](checklist-core.md) -- every item your executor must satisfy
- [Extension Checklist](checklist-extensions.md) -- extension system requirements
- [Packaging Rules](packaging.md) -- validator/executor separation
- [Conformance Levels](conformance-levels.md) -- omission options and outcome labels
- [Block Editor Mapping](block-editor.md) -- UI slot mapping for visual editors
- [Grammar Notes](notes/grammar.md) -- ANTLR4 grammar implementation notes
- [Extension Notes](notes/extensions.md) -- extension format conventions
