# Changelog

## 0.9.7 -- Documentation sweep, laceNotifications fixes, harness alignment

- `laceNotifications` 1.1.0: a bare `{ "404": …, "default": … }` object in a `notification` option now resolves exactly like `op_map({ … })` (it previously emitted nothing, and suppressed the default notification too); a custom `structured(...)` notification is emitted as-is; a script-declared `timeout.notification` is honoured -- the rule reads `call.config.timeout.extensions.notification`, which is where every executor records extension fields on call-config sub-objects (now stated in lace-spec §3.2 / §9.2 and lace-extensions §3.1 / §8.3); `silentOnRepeat` defaults to `true` even when an `options {}` block is present without the key; assert conditions are matched to the previous run by `(method: "assert", index)` instead of by position in `assertions[]`. Six new vectors pin these.
- Hard-fail cascade stated as implemented (lace-spec §4.1, §7, checklist §7/§10): after a failed `.expect()`, the call's `.check()` and `.assert()` blocks are still evaluated and recorded; `.store()` / `.wait()` on that call and all later calls are skipped. New core vector `check_and_assert_evaluated_after_expect_fail`.
- Lexer errors are `PARSE_ERROR` (new parse vector `lexer_error_is_parse_error`). All three validators raised an uncaught lexer exception on unrecognised characters such as `==`.
- lace-extensions: `compare()` returns `lt` / `eq` / `gt` / `neq` only, so `lte` / `gte` `op_map` keys never match (§7); function parameters are bare identifiers (§6, laceext schema); `[types]` is a top-level section (§2); the §2.1 TOML example wrongly called a multi-line array of single-line inline tables invalid; `call.error` added to the `on call` context (§8.3); an extension may read its own `runVars` back in `on script` (§9); rule-body comments are `#`; `%` is not a rule-language operator; §12 describes the real laceBaseline output and adds §12.2 `laceEmitRecovery`; activation (`[executor].extensions` / `--enable-extension`) vs configuration (`[extensions.<name>]`) made explicit (§11, lace-spec §11).
- lace-spec: `dns` (not `dnsMs`) in §4.1 / §13; §13 presets use `op_map` with reachable keys and the correct chain order; `scope_name` includes `redirects` and `store_key` admits `$name` (§2.1); `cookieJar` default `inherit`; `redirects.max` / `timeout.ms` over the system maximum are validation errors; §9.2 example fixed (valid JSON, `dns` / `tls` objects, `bodyNotCapturedReason`, `config.*.extensions`); §10 lists all twelve hooks; §11 drops the nonexistent `laceLogging`, adds `executor.user_agent`, `--enable-extension`, `--bodies-dir`, `--env`; §12 table lists every registry code; CLI names are `lacelang-executor` / `lacelang-validate`.
- Grammar (`lacelang.g4`): empty scope / assert / store lists parse so the validator can report `EMPTY_*_BLOCK` (the conformance vectors already required this); the header no longer claims interpolation is lexed. `grammar-tests/positive/metric-increment.lace` used `==`.
- Schemas and registry: `$id`s follow `VERSION`; AST `version` const is `0.9.4` and is documented as the AST-format version (bumped only when the AST shape changes); `exposed` is allowed on `[functions.X]`; the conformance-vector schema now describes `lace_config`, `env`, `cli_args`, `extensions` (names), `no_default_ignores`, `optional`, structured variable values and an optional `http_mock`, matching what the C harness reads; `specs/tools` look schemas up by their `$id` (both checkers had been failing silently on the old hard-coded ids); `EXT_EMIT_FORBIDDEN_TARGET` / `EXT_RUN_VAR_NAMESPACE` are runtime warnings; snake_case leftovers in descriptions removed. Three vector ids normalised to snake_case.
- Examples regenerated with executor 0.2.0 (`config` resolved, `bodyPath: null` + `bodyNotCapturedReason`); bundled extension examples corrected (`check_with_opmap`, `timeout_notification`, `silent_on_repeat`); README and every wiki page brought in line with the spec, including the 0.9.4 -- 0.9.6 changes the wiki had missed.

## 0.9.6 -- Assert expression strings must re-parse

- The `assertions[].expression` source rendering (spec §9.2) quotes object-literal keys that are not bare identifiers (`404:`, `content-type:` were printed bare and did not re-parse). Fixes the round-trip contract of every implementation's expression formatter.
- Added a conformance vector pinning the quoted form across executors.

## 0.9.5 -- Script-declared recovery notifications

- `laceEmitRecovery` 1.1.0: the recovery notification can be declared in the script itself via a `recovery` option on any expect/check scope or assert condition -- `options: { recovery: { notification: template("back-up") } }`. A bare string is shorthand for `text(...)`. Precedence: script-declared > config `notification` > config `recovery_message`; when several scopes declare one, the last evaluated declaration wins. The declared value surfaces in `runVars` as `laceEmitRecovery.recoveryNotification`.
- Corrected the extension config docs: `notification` overrides use the TOML table form (`{ tag = "template", name = "..." }`) -- the previously shown `template(...)` call syntax is not valid TOML.
- Added four conformance vectors: scope declaration, bare-string shorthand, assert-condition declaration, and script-beats-config precedence.

## 0.9.4 -- count() and includes() assert functions

- Added two core functions usable only inside `.assert()` conditions: `count(x)` (element count when `x` is an array, otherwise `1`) and `includes(search, x)` (true when the raw-string form of `x` contains `search`, i.e. `LIKE %search%`).
- Outside an `.assert()` condition they are a validation error (`UNKNOWN_FUNCTION`); a wrong argument count is `FUNC_ARG_TYPE`. No grammar or AST-shape change -- they parse as ordinary function calls.
- Added conformance vectors covering the assert-context gate, arity, and execution semantics.

## 0.9.3 -- Connection-error notifications

- `laceNotifications` now emits a default `structured` notification when a call fails with a connection-level error (DNS failure, connection refused, TLS error, etc.). These failures never reach assertion evaluation, so the existing scope and condition rules could not surface them.
- The notification fires once on entry into the error state -- when the same call had no error on the previous run, or there is no previous run -- and stays silent while the error persists. Timeouts are unaffected; their existing rules still own that outcome.
- Added conformance vectors covering the default error notification and its silent-on-repeat behaviour.

## 0.9.2 -- laceNotifications, laceEmitRecovery

- Fixed laceNotifications `silentOnRepeat` default behavior
- Extended laceNotifications tests to cover the `silentOnRepeat` functionality
- Added `laceEmitRecovery` extension with test coverage

## 0.9.1 -- Body saving changes

- Removed `bodyPath` from the request record schema; request bodies are no longer saved to disk (they are already present in the AST)
- Added `result.bodies.dir` configuration option (default `false`) to control whether response body files are written
- Added `--save-body` CLI flag to enable response body file writing for a single run
- Body file path convention simplified to `call_{index}_response.{ext}`

## 0.9.0 -- Initial specifications

First public release of the Lace probe scripting language.

- Prose specification (`lace-spec.md`)
- Extension system specification (`lace-extensions.md`)
- ANTLR4 grammars (`lacelang.g4`, `laceext.g4`)
- JSON schemas for AST, ProbeResult, `.laceext`, `lace.config`, executor manifest, and conformance vectors
- Error code registry (`error-codes.json`)
- Conformance testkit with C harness and 206<!-- vc --> test vectors
- Extension DSL with `set` statement for mutable bindings in function bodies
- Bundled default extensions: `laceNotifications`, `laceBaseline`
- Test extensions: `hookTrace`, `notifRelay`, `notifCounter`, `notifWatch`, `badNamespace`, `configDemo`
- Example `.lace` scripts for notifications and baseline spike detection
- Justification document (`justification.md`)
