# Error Codes

Canonical error and warning codes emitted by every Lace validator. Cross-implementation conformance depends on every validator emitting the same code for the same condition. The registry is `specs/error-codes.json`; the rules behind each code are in the specification (§12) and the extension specification.

Spec version: 0.9.7<!-- sv -->

Errors block activation: a script with any error does not run. Warnings are reported and execution proceeds. `EXT_EMIT_FORBIDDEN_TARGET` and `EXT_RUN_VAR_NAMESPACE` are raised at runtime: the offending emit is dropped and the warning is recorded in the call's `warnings` array.

## Parsing

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `PARSE_ERROR` | error | Yes | Source text does not conform to the grammar. |

## Structure

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `AT_LEAST_ONE_CALL` | error | Yes | Script contains zero HTTP calls. |
| `EMPTY_CHAIN` | error | Yes | A call has zero chain methods. |
| `CHAIN_ORDER` | error | Yes | Chain methods appear in wrong order. Required: `.expect` -> `.check` -> `.assert` -> `.store` -> `.wait`. |
| `CHAIN_DUPLICATE` | error | Yes | A chain method appears more than once on the same call. |
| `EMPTY_SCOPE_BLOCK` | error | Yes | `.expect()` or `.check()` present with zero scopes. |
| `EMPTY_ASSERT_BLOCK` | error | Yes | `.assert()` present with zero conditions. |
| `EMPTY_STORE_BLOCK` | error | Yes | `.store()` present with zero entries. |
| `THIS_OUT_OF_SCOPE` | error | Yes | `this` referenced outside chain methods or in a different call's chain. |
| `PREV_WITHOUT_RESULTS` | warning | No | `prev.*` referenced but no `--prev-results` provided. |
| `HIGH_CALL_COUNT` | warning | No | Script contains more than 10 calls. |

## Functions and Expressions

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `UNKNOWN_FUNCTION` | error | Yes | Function call to a name not in the helper set (`json`/`form`/`schema`, plus `count`/`includes` inside an `.assert()` condition) outside an extension-permitted context. |
| `FUNC_ARG_TYPE` | error | Yes | Helper function called with wrong argument type or arity (e.g. `json(string)` instead of `json(objectLit)`, `schema(literal)` instead of `schema($var)`, or `count`/`includes` with the wrong number of arguments). |
| `EXPRESSION_SYNTAX` | error | Yes | `.assert()` expression failed to parse. (A non-integer `.wait()` argument is a `PARSE_ERROR`, not this code.) |

## Variables

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `VARIABLE_UNKNOWN` | error | Yes | `$var` reference does not appear in the variable registry. |
| `RUN_VAR_REASSIGNED` | error | Yes | Same `$$var` assigned more than once across the script. |
| `SCHEMA_VAR_UNKNOWN` | error | Yes | `schema($var)` argument is not a declared variable. |

## Redirects and Timeout

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `REDIRECTS_MAX_LIMIT` | error | Yes | `redirects.max` exceeds context system maximum. |
| `TIMEOUT_MS_LIMIT` | error | Yes | `timeout.ms` exceeds context system maximum. |
| `TIMEOUT_RETRIES_REQUIRES_RETRY` | error | Yes | `timeout.retries` set without `timeout.action: "retry"`. |
| `TIMEOUT_ACTION_INVALID` | error | Yes | `timeout.action` value is not one of `"fail"`, `"warn"`, `"retry"`. |

## Cookies

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `CLEAR_COOKIES_WRONG_JAR` | error | Yes | `clearCookies` present but `cookieJar` is not a `selective_clear` variant. |
| `COOKIE_JAR_NAMED_EMPTY` | error | Yes | `cookieJar = "named:"` with empty name. |
| `COOKIE_JAR_FORMAT` | error | Yes | `cookieJar` value does not match a recognised pattern. |

## Scope Values

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `OP_VALUE_INVALID` | error | Yes | `op` value is not one of: `lt`, `lte`, `eq`, `neq`, `gte`, `gt`. |
| `MAX_BODY_FORMAT` | error | Yes | `bodySize` scope value does not match `/\d+(k|kb|m|mb|g|gb)?/i`. (Code name kept from the scope's former name, `maxBody`.) |

## Extensions

| Code | Severity | Blocks Activation | Description |
|---|---|---|---|
| `EXT_FIELD_INACTIVE` | warning | No | Extension-registered field used but the registering extension is not active. |
| `EXT_HOOK_NAME_UNKNOWN` | error | Yes | Extension rule registers for an unknown hook name. |
| `EXT_EMIT_FORBIDDEN_TARGET` | warning | No | Extension rule emitted to a target not declared in `[result]` or to a forbidden core path. Runtime: the emit is rejected, this warning is recorded on the call, execution continues. |
| `EXT_RUN_VAR_NAMESPACE` | warning | No | Extension emit to `result.runVars` used a key not prefixed with the extension name. Runtime: the emit is rejected, this warning is recorded on the call, execution continues. |
