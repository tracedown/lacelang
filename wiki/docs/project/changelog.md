# Changelog

## 0.9.3 -- Connection-Error Notifications

- Notify on connection-level call errors ([#22](https://github.com/tracedown/lacelang/commit/a1702025836a0d0d70daf921bbc199d042672675))

## 0.9.2 -- laceNotifications, laceEmitRecovery

- Fixed laceNotifications `silentOnRepeat` default behavior
- Extended laceNotifications tests to cover the `silentOnRepeat` functionality
- Added `laceEmitRecovery` extension with test coverage

## 0.9.1 -- Body Saving Changes

- Removed `bodyPath` from the request record schema; request bodies are no longer saved to disk (they are already present in the AST)
- Added `result.bodies.dir` configuration option (default `false`) to control whether response body files are written
- Added `--save-body` CLI flag to enable response body file writing for a single run
- Body file path convention simplified to `call_{index}_response.{ext}`

## 0.9.0 -- Initial Specifications

First public release of the Lace probe scripting language.

- Prose specification
- Extension system specification
- ANTLR4 grammars (`lacelang.g4`, `laceext.g4`)
- JSON schemas for AST, ProbeResult, `.laceext`, `lace.config`, executor manifest, and conformance vectors
- Error code registry (`error-codes.json`)
- Conformance testkit with C harness and 186<!-- vc --> test vectors
- Extension DSL with `set` statement for mutable bindings in function bodies
- Bundled default extensions: `laceNotifications`, `laceBaseline`
- Test extensions: `hookTrace`, `notifRelay`, `notifCounter`, `notifWatch`, `badNamespace`, `configDemo`
- Example `.lace` scripts for notifications and baseline spike detection
- Justification document (`justification.md`)
