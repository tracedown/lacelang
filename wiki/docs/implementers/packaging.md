# Packaging

Every Lace implementation **must** ship its language validator and its runtime executor as **two distinct, independently-installable packages**. One must not force-install the other.

Spec version: 0.9.1 (source: `lace-spec.md` section 16)

## Package Roles

| Package | Provides | Depends on | Typical Consumers |
|---|---|---|---|
| **Validator** | Lexer, parser, semantic checks, canonical error codes, CLI with `parse` and `validate` subcommands | Nothing (zero network surface) | CI jobs, IDE/editor plugins, script-authoring tools, backend platform validators |
| **Executor** | Runtime (HTTP client, assertion evaluation, cookie jars, extension dispatch, body storage), CLI with `parse` / `validate` / `run` subcommands | The validator package | Probe runners, monitoring fleets, any platform hosting Lace |

## Rules

- The validator package **must not** link an HTTP client, TLS stack, DNS resolver, cookie library, notification dispatcher, or any network-capable dependency. Installing it must be safe in sandboxed environments (read-only filesystems, no egress, air-gapped CI).

- The executor package **must** depend on the validator package at the same spec-compatible version and delegate `parse` + `validate` subcommands to it. It must not re-implement parsing or validation logic.

- Both packages share the same spec version.

- Both may share a monorepo or live in separate repositories. The choice is an implementation detail; the **package** separation is mandatory.

- The executor's CLI **must** expose all three conformance subcommands (`parse`, `validate`, `run`) so the testkit harness can drive it end-to-end with a single `-c <cmd>` flag.

- The validator's CLI **must** expose `parse` and `validate`, and **must not** expose `run`.

- **Package naming:** the spec does not mandate a convention, but the canonical suggestion is `lacelang-validator-<lang>` and `lacelang-executor-<lang>` (e.g. `lacelang-validator` / `lacelang-executor` on PyPI, `@lacelang/validator` / `@lacelang/executor` on npm).

## Rationale

Most consumers of `.lace` source text do not run probes. IDE linters, CI gates, and backend platform validators need syntax and semantic checks but have no reason to depend on an HTTP stack. Forcing the runtime into every dependency tree enlarges the supply-chain surface and excludes constrained environments. Conversely, some runners may want to validate, then hand off execution to a remote worker -- that worker, too, should be able to install the validator alone.

The split also makes conformance auditing easier: a validator package is a pure function from text to diagnostics, so its correctness is tractably unit-testable without network mocks or subprocess sandboxing.
