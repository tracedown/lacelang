# Core Executor Compatibility Checklist

> Spec version: 0.9.0
> Companion to: [lace-spec.md](./lace-spec.md)

An executor implementation is considered **Lace Core Compatible** when it satisfies every item in this checklist. Partial compatibility must be documented — an executor may declare which sections it supports and which it does not.

This checklist covers the core language only. Extension system compatibility is defined separately in **[checklist-extensions.md](./checklist-extensions.md)**.

---

## 1. Parsing

- [ ] Parses `.lace` source text against the formal grammar in §2.1
- [ ] Applies lexical rules from §2.2: `$var`, `$$var` tokenisation, string escape sequences, comment stripping
- [ ] Rejects source text that does not conform to the grammar with a structured parse error including line and column
- [ ] Accepts trailing commas in all list and object positions where the grammar permits them
- [ ] Parses all five HTTP methods: `get`, `post`, `put`, `patch`, `delete`
- [ ] Parses call config sub-objects: `redirects`, `security`, `timeout`
- [ ] Parses unknown fields in `redirects`, `security`, `timeout`, and call root as extension fields — does not reject them at parse time
- [ ] Parses all chain methods in any valid subset and order
- [ ] Parses all three scope forms: shorthand, value+op, full form with `options {}`
- [ ] Parses `options {}` content as free-form key-value pairs — does not reject unknown keys at parse time
- [ ] Parses all three body match forms in `.expect()`/`.check()`: `schema($var)`, string literal, variable ref
- [ ] Parses `.assert()` with both `expect` and `check` clauses, shorthand and full condition forms
- [ ] Parses `prev` dot-access and array-index expressions
- [ ] Parses `this` dot-access expressions
- [ ] Produces an internal AST from valid source — the AST is an implementation detail and is never stored or transmitted

---

## 2. Validation

All validation rules from lace-spec.md §12 must be enforced. The following are the behavioural requirements:

- [ ] Reports all validation errors before execution — does not stop at the first error
- [ ] Distinguishes **errors** (block execution) from **warnings** (allow execution with notice)
- [ ] Requires at least one HTTP call per script
- [ ] Requires at least one chain method per call
- [ ] Enforces chain method order: `.expect → .check → .assert → .store → .wait`
- [ ] Rejects duplicate chain methods on the same call
- [ ] Rejects `.expect()` and `.check()` with zero scopes
- [ ] Rejects `this.*` references outside a chain method body (cross-call references are not constructible — each call's chain is its own parser-level scope)
- [ ] Rejects function calls in expressions that are not `json`, `form`, or `schema`
- [ ] Validates all `$var` references against the provided variable registry — rejects unknown references
- [ ] Rejects `$$var` assigned more than once across the script
- [ ] Rejects `schema($var)` where the variable is absent from the registry
- [ ] Validates expression syntax in all `.assert()` conditions
- [ ] Rejects `redirects.max` values exceeding the context system maximum
- [ ] Rejects `timeout.ms` values exceeding the context system maximum
- [ ] Rejects `timeout.retries` without `timeout.action: "retry"`
- [ ] Rejects `clearCookies` when `cookieJar` is not a `selective_clear` variant
- [ ] Rejects `cookieJar: "named:"` with an empty name
- [ ] Rejects `op` values not in: `lt`, `lte`, `eq`, `neq`, `gte`, `gt`
- [ ] Rejects malformed `bodySize` size strings
- [ ] Emits a warning for unknown fields when the registering extension is not active
- [ ] Emits a warning for `prev.*` references when `--prev-results` is not provided
- [ ] Emits a warning when the script contains more than 10 calls
- [ ] Accepts `options {}` with unknown field names regardless of extension state — does not error on unknown option keys

---

## 3. Variable Handling

- [ ] Accepts a flat key-value map as the variable injection input (`--vars` or execution context)
- [ ] `$varname` resolves to its value from the injected map
- [ ] `$var` absent from the injected map resolves to `null`
- [ ] `$$var` not yet assigned resolves to `null`
- [ ] `$$var` assigned via `.store()` is available to all subsequent chain methods and calls in the same run
- [ ] Enforces write-once: a second `.store()` assignment to the same `$$` key is rejected at validation time, not at runtime
- [ ] `$var` in `.store()` goes to `actions.variables` as a write-back; `$` prefix is stripped from the key in the result; the variable's in-memory value does not change during the current run
- [ ] All `$$var` final values are present in `runVars` in the result, each key appearing exactly once
- [ ] `runVars` contains only keys that were actually assigned — unassigned `$$var` references do not appear

---

## 4. Null Semantics

- [ ] Missing `$var` from injected map → `null`
- [ ] Unassigned `$$var` → `null`
- [ ] `prev` when no previous results provided → `null`
- [ ] Any field access on `null` → `null` (no exception thrown)
- [ ] `null` in string interpolation → the literal string `"null"` with a warning recorded in the call's `warnings` array
- [ ] `null eq null` → `true`
- [ ] `null eq non_null_value` → `false`
- [ ] `null neq non_null_value` → `true`
- [ ] `null` as operand of `lt`, `gt`, `lte`, `gte` → outcome `"indeterminate"` (no exception, no hard fail)
- [ ] `null` as operand of `+`, `-`, `*`, `/` → `null` result (no exception)
- [ ] `schema($var)` where `$var` resolves to `null` → hard fail
- [ ] `null` stored via `.store()` → valid; appears in `runVars` or `actions.variables` as JSON `null`

---

## 5. HTTP Execution

- [ ] Executes calls sequentially in script order
- [ ] Makes real HTTP requests for each call (not simulated)
- [ ] Applies all resolved request config: headers, body, cookies, cookieJar, redirects, security, timeout
- [ ] Performs variable interpolation in `url`, `headers` values, and `body` string values before sending
- [ ] Records `startedAt` and `endedAt` timestamps per call in UTC ISO 8601
- [ ] Captures all timing fields: `responseTimeMs`, `dnsMs`, `connectMs`, `tlsMs`, `ttfbMs`, `transferMs`, `sizeBytes`
- [ ] Sets `tlsMs` to `0` for non-HTTPS calls
- [ ] Parses response body as JSON when `Content-Type` is `application/json`; raw string otherwise
- [ ] Lower-cases all response header keys
- [ ] Enforces `redirects.max` — hard fails when exceeded regardless of other config
- [ ] When `security.rejectInvalidCerts: true`: hard fails on any TLS error
- [ ] When `security.rejectInvalidCerts: false`: records TLS error as a warning, continues execution
- [ ] Enforces `timeout.ms` per call
- [ ] On timeout with `timeout.action: "fail"`: hard fails
- [ ] On timeout with `timeout.action: "warn"`: records soft fail, continues
- [ ] On timeout with `timeout.action: "retry"`: retries up to `timeout.retries` times; hard fails after all attempts exhausted
- [ ] `timeout.retries: N` means N+1 total attempts (1 initial + N retries)

---

## 6. Cookie Jar

- [ ] `"inherit"`: continues previous call's jar; first call starts with empty jar
- [ ] `"fresh"`: starts this call with an empty jar, discarding all previous cookies
- [ ] `"selective_clear"`: clears only the keys in `clearCookies` from the default jar before this call
- [ ] `"named:{name}"`: uses an isolated jar identified by `name`; creates it if it does not exist
- [ ] `"{name}:selective_clear"`: clears keys in `clearCookies` from the named jar `{name}`
- [ ] Named jars persist across calls within a run but not between runs
- [ ] Explicit `cookies` object values are merged with the active jar at send time
- [ ] `Set-Cookie` response headers update the active jar after each call

---

## 7. Chain Method Execution

**`.expect()`**

- [ ] Evaluates **all** scopes before triggering failure cascade — does not stop at first failing scope
- [ ] Records every failing scope in `assertions[]`
- [ ] After all scopes evaluated: if any failed, triggers hard fail cascade
- [ ] Applies default operator per scope name (lace-spec.md §4.4) when `op` is omitted
- [ ] Passes `options {}` object through to `assertions[].options` opaquely — does not interpret it
- [ ] `status` with array value passes when actual status matches any element
- [ ] `tls` scope skipped (not evaluated) when `this.tls eq 0`

**`.check()`**

- [ ] Evaluates **all** scopes before recording failures — does not stop at first failing scope
- [ ] Records every failing scope in `assertions[]`
- [ ] After all scopes evaluated: execution continues regardless of outcome
- [ ] Same operator and `options` pass-through rules as `.expect()`

**`.store()`**

- [ ] Skipped entirely when any preceding chain method (`.expect()` or `.assert({ expect: [...] })`) produced a hard fail on the same call
- [ ] `$$key` writes to run-scope, available to subsequent calls
- [ ] `$key` and plain keys go to `actions.variables` in the result; `$` prefix is stripped from the key
- [ ] `$key` write-back does not change the variable's in-memory value during the current run
- [ ] Stored values may be any JSON-serialisable shape (scalar, object, or array)
- [ ] `null` is a valid stored value

**`.assert()`**

- [ ] Evaluates **all** `expect` conditions before triggering hard fail cascade
- [ ] Evaluates **all** `check` conditions regardless of outcome
- [ ] Records each condition outcome (`"passed"`, `"failed"`, `"indeterminate"`) in `assertions[]`
- [ ] Records `actualLhs` and `actualRhs` for each condition
- [ ] Records `expression` string for each condition
- [ ] Passes `options {}` per condition through to `assertions[].options` opaquely
- [ ] Null operand in ordered comparison or arithmetic → `"indeterminate"` outcome, no error, execution continues
- [ ] Hard fail cascade triggers after all `expect` conditions evaluated, if any failed

**`.wait()`**

- [ ] Pauses execution for the specified number of milliseconds after all other chain methods complete
- [ ] Is always the last chain method executed on a call

---

## 8. Body Matching

- [ ] `body: schema($var)` — parses the variable's value as a JSON Schema document and validates the response body against it
- [ ] `body: schema($var)` where `$var` is `null` — hard fail
- [ ] `body: schema($var)` where response body is not valid JSON — hard fail
- [ ] `body: "literal"` — compares raw response body string to the literal, case-sensitive exact match
- [ ] `body: $var` — compares raw response body string to the runtime value of the variable

---

## 9. `prev` Access

- [ ] `prev` resolves to the provided previous result object when `--prev-results` is supplied
- [ ] `prev` resolves to `null` when no previous results are provided
- [ ] All `prev.*` field access follows null propagation rules — no exception thrown on any access path
- [ ] `prev.calls[n]` accesses the nth call record from the previous result
- [ ] `prev.runVars.key` accesses the named run-scope variable from the previous result
- [ ] `prev.outcome` accesses the overall outcome of the previous result

---

## 10. Failure Cascade

- [ ] Hard fail on a call skips all remaining chain methods on that call (`.store()` included)
- [ ] Hard fail on a call marks all subsequent calls as `"skipped"` in the result
- [ ] Soft failures do not stop execution — the next chain method runs
- [ ] Notification events (if extension active) from soft failures in earlier calls are still included in the result even after a later hard fail
- [ ] `"skipped"` calls have `null` for `request`, `response`, and an empty `assertions` array

---

## 11. Result Structure

- [ ] Result contains: `outcome`, `startedAt`, `endedAt`, `elapsedMs`, `runVars`, `calls`, `actions`
- [ ] `outcome` is `"success"` when no hard fails occurred; `"failure"` on hard fail; `"timeout"` when the cause was a timeout
- [ ] `startedAt` is the timestamp before the first call begins; `endedAt` is after all chain methods complete or after cascade stops
- [ ] `runVars` is a flat object containing all assigned `$$var` keys and their final values
- [ ] `calls` contains one record per call in script order, including skipped calls
- [ ] Each call record contains: `index`, `outcome`, `startedAt`, `endedAt`, `request`, `response`, `assertions`, `config`, `warnings`, `error`
- [ ] `request` contains: `url` (resolved), `method`, `headers` (resolved), `bodyPath`
- [ ] `response` is `null` for skipped, timeout (no response received), or connection failure
- [ ] `response` contains: `status`, `statusText`, `headers`, `bodyPath`, and all timing fields
- [ ] `assertions` contains one entry per evaluated scope and condition, in evaluation order
- [ ] Each assertion entry contains: `method`, `outcome`, and type-appropriate fields (`scope`/`op`/`actual`/`expected` for scope assertions; `kind`/`index`/`expression`/`actualLhs`/`actualRhs` for assert conditions)
- [ ] `assertions[].options` contains the raw `options {}` object from source — `null` if no `options {}` was present
- [ ] `config` contains the resolved call config (after defaults applied), including any extension-registered fields passed through
- [ ] `warnings` is an array of strings — empty array when no warnings, never `null`
- [ ] `error` is a string describing non-assertion failure (connection error, TLS, redirect limit, body too large) or `null`
- [ ] `actions` is always present, even if empty
- [ ] `actions.variables` is present and contains all write-back `.store()` key-value pairs (`$name` and plain keys) when any write-back targets exist; `$` prefix stripped from `$name` keys; absent or empty object otherwise
- [ ] All result values are JSON-serialisable

---

## 12. Body Storage

- [ ] Writes request body to a file on the shared filesystem volume before sending the request
- [ ] Writes response body to a file on the shared filesystem volume after receiving the response
- [ ] File paths follow the convention: `{run_base_dir}/call_{index}_{request|response}.{ext}`
- [ ] Result JSON contains absolute file paths in `request.bodyPath` and `response.bodyPath`
- [ ] No body bytes appear in the result JSON itself
- [ ] `bodyPath` is `null` when a body was not captured
- [ ] When `bodyPath` is `null`, `bodyNotCapturedReason` is present on the containing object with value `"bodyTooLarge"`, `"notRequested"`, or `"timeout"`
- [ ] Run base directory is taken from execution context (configured via `result.bodies.dir`)

---

## 13. Configuration

- [ ] Reads `lace.config` TOML from script directory or working directory at startup
- [ ] All config values have defaults — executor runs without a config file
- [ ] Config resolution order: CLI flags → script-dir config → working-dir config → defaults
- [ ] `--config path` overrides config file location
- [ ] `--vars vars.json` loads variable injection map from JSON file
- [ ] `--var KEY=VALUE` injects a single variable; multiple flags merge; overrides `--vars`
- [ ] `--prev-results path` loads previous result JSON
- [ ] `--save-to path` overrides `result.path` for this run
- [ ] `env:VARNAME` config values resolved from environment at startup; error if variable unset
- [ ] `env:VARNAME:default` resolved from environment; uses `default` if variable unset
- [ ] `LACE_ENV` environment variable or `--env flag` selects the active `[lace.config.{env}]` section
- [ ] Saves result JSON to `result.path` after execution (directory: timestamped filename; file path: overwrites)
- [ ] `result.path = false` disables result saving

---

## 14. Extension Interface (Core Side)

These are the core executor's obligations toward the extension system. Full extension system compatibility is in [checklist-extensions.md](./checklist-extensions.md).

- [ ] Loads `.laceext` files listed in `executor.extensions` at startup; fails with clear error if a file is not found
- [ ] Passes `options {}` objects through to `assertions[].options` in the result without modification
- [ ] Passes extension-registered call config fields through to `calls[n].config` in the result without modification
- [ ] Fires all twelve hook points at the correct moments: `on before script`, `on script`, `on before call`, `on call`, `on before expect`, `on expect`, `on before check`, `on check`, `on before assert`, `on assert`, `on before store`, `on store`
- [ ] Provides the correct context object at each hook point as defined in `lace-extensions.md §8`
- [ ] Enforces extension variable namespace: rejects `emit result.runVars` keys not prefixed with the extension name
- [ ] Emits unknown-field warnings (not errors) for extension fields in source when the extension is not active
