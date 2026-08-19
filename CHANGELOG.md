# Changelog

## 0.9.5 - Script-declared recovery notifications

- `laceEmitRecovery` 1.1.0: the recovery notification can be declared in the script itself via a `recovery` option on any expect/check scope or assert condition — `options: { recovery: { notification: template("back-up") } }`. A bare string is shorthand for `text(...)`. Precedence: script-declared > config `notification` > config `recovery_message`; when several scopes declare one, the last evaluated declaration wins. The declared value surfaces in `runVars` as `laceEmitRecovery.recoveryNotification`.
- Corrected the extension config docs: `notification` overrides use the TOML table form (`{ tag = "template", name = "..." }`) — the previously shown `template(...)` call syntax is not valid TOML.
- Added four conformance vectors: scope declaration, bare-string shorthand, assert-condition declaration, and script-beats-config precedence.

## 0.9.4 - count() and includes() assert functions

- Added two core functions usable only inside `.assert()` conditions: `count(x)` (element count when `x` is an array, otherwise `1`) and `includes(search, x)` (true when the raw-string form of `x` contains `search`, i.e. `LIKE %search%`).
- Outside an `.assert()` condition they are a validation error (`UNKNOWN_FUNCTION`); a wrong argument count is `FUNC_ARG_TYPE`. No grammar or AST-shape change — they parse as ordinary function calls.
- Added conformance vectors covering the assert-context gate, arity, and execution semantics.

## 0.9.3 - Connection-error notifications

- `laceNotifications` now emits a default `structured` notification when a call fails with a connection-level error (DNS failure, connection refused, TLS error, etc.). These failures never reach assertion evaluation, so the existing scope and condition rules could not surface them.
- The notification fires once on entry into the error state — when the same call had no error on the previous run, or there is no previous run — and stays silent while the error persists. Timeouts are unaffected; their existing rules still own that outcome.
- Added conformance vectors covering the default error notification and its silent-on-repeat behaviour.

## 0.9.2 - laceNotifications, laceEmitRecovery

- Fixed laceNotifications `silentOnRepeat` default behavior
- Extended laceNotifications tests to cover the `silentOnRepeat` functionality
- Added `laceEmitRecovery` extension with test coverage

## 0.9.1 — Body saving changes

- Removed `bodyPath` from the request record schema; request bodies are no longer saved to disk (they are already present in the AST)
- Added `result.bodies.dir` configuration option (default `false`) to control whether response body files are written
- Added `--save-body` CLI flag to enable response body file writing for a single run
- Body file path convention simplified to `call_{index}_response.{ext}`

## 0.9.0 — Initial specifications

First public release of the Lace probe scripting language.

- Prose specification (`lace-spec.md`)
- Extension system specification (`lace-extensions.md`)
- ANTLR4 grammars (`lacelang.g4`, `laceext.g4`)
- JSON schemas for AST, ProbeResult, `.laceext`, `lace.config`, executor manifest, and conformance vectors
- Error code registry (`error-codes.json`)
- Conformance testkit with C harness and 198<!-- vc --> test vectors
- Extension DSL with `set` statement for mutable bindings in function bodies
- Bundled default extensions: `laceNotifications`, `laceBaseline`
- Test extensions: `hookTrace`, `notifRelay`, `notifCounter`, `notifWatch`, `badNamespace`, `configDemo`
- Example `.lace` scripts for notifications and baseline spike detection
- Justification document (`justification.md`)
