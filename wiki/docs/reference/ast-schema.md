# AST Schema

The Lace AST is the canonical representation produced by parsers from `.lace` source text (`specs/schemas/ast.json`).

!!! note
    The AST is the wire format between validator and executor, and the shape the testkit compares in `parse` conformance vectors -- every implementation's `parse` subcommand must serialise its AST to this shape. It must be JSON-serialisable, with no language-specific sentinels.

Spec version: 0.9.7<!-- sv -->

## Top-Level

| Field | Type | Required | Description |
|---|---|---|---|
| `version` | `"0.9.4"` | Yes | AST format version. It changes only when the AST shape changes, independently of the spec version -- so it can lag the spec version shown above. |
| `calls` | array of [Call](#call) (min 1) | Yes | Ordered HTTP calls. Validator enforces minimum 1 call. |

## Call

| Field | Type | Required | Description |
|---|---|---|---|
| `method` | `"get"` \| `"post"` \| `"put"` \| `"patch"` \| `"delete"` | Yes | HTTP method. |
| `url` | string (min 1 char) | Yes | Request URL with optional variable interpolation (left in the string, resolved at evaluation time). |
| `config` | [CallConfig](#callconfig) | No | Resolved request config. |
| `chain` | [Chain](#chain) | Yes | Chain methods. At least one must be present. |

## CallConfig

All fields optional. Extension-registered fields go in `extensions`.

| Field | Type | Description |
|---|---|---|
| `headers` | object (string keys -> [Expr](#expression-types)) | Header name to value expression. |
| `body` | [BodyValue](#bodyvalue) | Request body. |
| `cookies` | object (string keys -> Expr) | Cookie key-value pairs. |
| `cookieJar` | string | Cookie jar mode: `inherit`, `fresh`, `selective_clear`, `named:<name>`, `<name>:selective_clear`. |
| `clearCookies` | array of string | Cookie names to clear. Valid only with `selective_clear` jar. |
| `redirects` | [Redirects](#redirects) | Redirect config. |
| `security` | [Security](#security) | Security config. |
| `timeout` | [Timeout](#timeout) | Timeout config. |
| `extensions` | object (string keys -> Expr) | Extension-registered call config fields. |

## BodyValue

Discriminated by `type`:

| Variant | `type` | `value` | Description |
|---|---|---|---|
| JSON | `"json"` | ObjectLitExpr | JSON body from `json({...})`. |
| Form | `"form"` | ObjectLitExpr | URL-encoded form from `form({...})`. |
| Raw | `"raw"` | string | Raw string body. |

## Redirects

| Field | Type | Description |
|---|---|---|
| `follow` | boolean | Whether to follow redirects. |
| `max` | integer (>= 0) | Maximum redirect hops. Validated against the context system maximum (`executor.maxRedirects`, default 10) -- `REDIRECTS_MAX_LIMIT`. |
| `extensions` | object (string keys -> Expr) | Extension-registered fields. |

## Security

| Field | Type | Description |
|---|---|---|
| `rejectInvalidCerts` | boolean | Whether to reject invalid TLS certificates. |
| `extensions` | object (string keys -> Expr) | Extension-registered fields. |

## Timeout

| Field | Type | Description |
|---|---|---|
| `ms` | integer (>= 1) | Timeout in milliseconds. |
| `action` | `"fail"` \| `"warn"` \| `"retry"` | Timeout behaviour. |
| `retries` | integer (>= 0) | Retry count (requires `action: "retry"`). |
| `extensions` | object (string keys -> Expr) | Extension-registered fields. |

## Chain

Fixed order enforced by the validator (`CHAIN_ORDER`): `.expect` -> `.check` -> `.assert` -> `.store` -> `.wait`. Each appears at most once. At least one must be present.

| Field | Type | Description |
|---|---|---|
| `expect` | [ScopeBlock](#scopeblock) | Hard-fail scope assertions. |
| `check` | [ScopeBlock](#scopeblock) | Soft-fail scope assertions. |
| `assert` | [AssertBlock](#assertblock) | Expression-based conditions. |
| `store` | [StoreBlock](#storeblock) | Variable storage. |
| `wait` | integer (>= 0) | Pause duration in ms. |

## ScopeBlock

Object with scope names as keys. Each scope appears at most once. Must have at least one entry -- the parser accepts an empty block and the validator rejects it (`EMPTY_SCOPE_BLOCK`).

Scope names: `status`, `body`, `headers`, `bodySize`, `totalDelayMs`, `dns`, `connect`, `tls`, `ttfb`, `transfer`, `size`, `redirects`.

Each scope value is a [ScopeValue](#scopevalue).

## ScopeValue

| Field | Type | Required | Description |
|---|---|---|---|
| `value` | Expr | Yes | The expected value expression. |
| `op` | `"lt"` \| `"lte"` \| `"eq"` \| `"neq"` \| `"gte"` \| `"gt"` | No | Comparison operator. When omitted, executor applies the scope's default. |
| `match` | `"first"` \| `"last"` \| `"any"` | No | Valid only on `redirects` scope. Default `"any"`. |
| `mode` | `"strict"` \| `"loose"` | No | Valid only on `body` scope with `schema(...)`. Default `"loose"`. |
| `options` | object (string keys -> Expr) | No | Extension-defined option fields. Core executor does not interpret. |

## AssertBlock

| Field | Type | Description |
|---|---|---|
| `expect` | array of [Condition](#condition) | Hard-fail conditions. |
| `check` | array of [Condition](#condition) | Soft-fail conditions. |

At least one of `expect` or `check` must be present. A clause with zero conditions (`expect: []`) parses and is rejected by the validator (`EMPTY_ASSERT_BLOCK`).

## Condition

| Field | Type | Required | Description |
|---|---|---|---|
| `condition` | Expr | Yes | The condition expression. |
| `options` | object (string keys -> Expr) | No | Extension-defined option fields. |

## StoreBlock

Object mapping store keys to [StoreEntry](#storeentry) values. Must have at least one entry -- the validator rejects an empty `.store({})` (`EMPTY_STORE_BLOCK`). Source keys are preserved as-is (e.g. `$$token`, `$cursor`, or `last_count`).

## StoreEntry

| Field | Type | Required | Description |
|---|---|---|---|
| `scope` | `"run"` \| `"writeback"` | Yes | Resolved from key prefix: `$$name` -> run; `$name` and plain keys -> writeback. |
| `value` | Expr | Yes | The value expression. |

## Expression Types

Expressions are discriminated by `kind`. All are recursive.

| Kind | Description | Key Fields |
|---|---|---|
| `binary` | Binary operation | `op` (`eq`, `neq`, `lt`, `lte`, `gt`, `gte`, `and`, `or`, `+`, `-`, `*`, `/`, `%`), `left`, `right` |
| `unary` | Unary operation | `op` (`not`, `-`), `operand` |
| `thisRef` | Response field access | `path` (array of strings, e.g. `["body", "token"]`) |
| `prevRef` | Previous result access | `path` (array of field/index segments) |
| `funcCall` | Function call | `name`, `args` (array of Expr). Core names: `json`, `form`, `schema` (any expression), plus `count`, `includes` (only inside an `.assert()` condition). Extensions register additional names; the validator enforces the context-appropriate restriction. |
| `scriptVar` | Script variable (`$name`) | `name` (without `$`), optional `path` |
| `runVar` | Run variable (`$$name`) | `name` (without `$$`), optional `path` |
| `literal` | Literal value | `valueType` (`string`, `int`, `float`, `bool`, `null`), `value`. String values carry the unescaped content; interpolation references stay in the string and are re-scanned at evaluation time. |
| `objectLit` | Object literal | `entries` (ordered key-value pairs) |
| `arrayLit` | Array literal | `items` (array of Expr) |

### VarPathSeg

Used in `scriptVar` and `runVar` path arrays:

| Variant | Fields | Description |
|---|---|---|
| Field access | `type: "field"`, `name: string` | Dot-notation field access. |
| Array index | `type: "index"`, `index: integer` | Bracket-notation array index. |

### PrevRef Path

Uses mixed field-access and array-index segments:

- `prev.calls[0].outcome` -> `[{type: "field", name: "calls"}, {type: "index", index: 0}, {type: "field", name: "outcome"}]`
