# Changelog

## 0.9.1 -- Body Saving Changes

- Removed `bodyPath` from the request record schema; request bodies are no longer saved to disk (they are already present in the AST)
- Added `result.bodies.dir` configuration option (default `false`) to control whether response body files are written
- Added `--save-body` CLI flag to enable response body file writing for a single run
- Body file path convention simplified to `call_{index}_response.{ext}`

## 0.9.0 -- Initial Specifications

First public release of the Lace probe scripting language.

- Prose specification (`lace-spec.md`)
- Extension system specification (`lace-extensions.md`)
- ANTLR4 grammars (`lacelang.g4`, `laceext.g4`)
- JSON schemas for AST, ProbeResult, `.laceext`, `lace.config`, executor manifest, and conformance vectors
- Error code registry (`error-codes.json`)
- Conformance testkit with C harness and 172 test vectors
- Extension DSL with `set` statement for mutable bindings in function bodies
- Bundled default extensions: `laceNotifications`, `laceBaseline`
- Test extensions: `hookTrace`, `notifRelay`, `notifCounter`, `notifWatch`, `badNamespace`, `configDemo`
- Example `.lace` scripts for notifications and baseline spike detection
- Justification document (`justification.md`)
