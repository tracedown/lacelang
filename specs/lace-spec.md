# Lace — Specification v0.9.0

> Status: Initial release (v0.9.0)
> Referenced by: api-monitoring-spec.md §3

## Table of Contents

1. [Overview](#1-overview)
2. [Grammar](#2-grammar)
   - 2.1 [Formal Grammar (EBNF)](#21-formal-grammar-ebnf)
   - 2.2 [Lexical Rules](#22-lexical-rules)
   - 2.3 [Structure Rules](#23-structure-rules)
3. [HTTP Call Syntax](#3-http-call-syntax)
   - 3.1 [Methods & URL](#31-methods--url)
   - 3.2 [Request Config](#32-request-config)
   - 3.3 [Cookie Jar](#33-cookie-jar)
   - 3.4 [Response Object (`this`)](#34-response-object-this)
   - 3.5 [Variable Interpolation in Strings](#35-variable-interpolation-in-strings)
4. [Chain Methods](#4-chain-methods)
   - 4.1 [`.expect()`](#41-expect)
   - 4.2 [`.check()`](#42-check)
   - 4.3 [Scope Form Reference](#43-scope-form-reference)
   - 4.4 [Default Operators](#44-default-operators)
   - 4.5 [Body Matching](#45-body-matching)
   - 4.6 [`.store()`](#46-store)
   - 4.7 [`.assert()`](#47-assert)
   - 4.8 [`.wait()`](#48-wait)
5. [Variables](#5-variables)
   - 5.1 [Script Variables (`$var`)](#51-script-variables-var)
   - 5.2 [Run-Scope Variables (`$$var`)](#52-run-scope-variables-var)
   - 5.3 [Immutability Rule](#53-immutability-rule)
   - 5.4 [Null Semantics](#54-null-semantics)
   - 5.5 [Injection Model](#55-injection-model)
6. [Previous Results (`prev`)](#6-previous-results-prev)
7. [Failure Semantics](#7-failure-semantics)
8. [Helper Functions](#8-helper-functions)
9. [Executor Result](#9-executor-result)
   - 9.1 [Result Structure](#91-result-structure)
   - 9.2 [Call Record](#92-call-record)
   - 9.3 [Actions](#93-actions)
   - 9.4 [Body Storage](#94-body-storage)
10. [Extension System (Overview)](#10-extension-system-overview)
11. [Configuration (`lace.config`)](#11-configuration-laceconfig)
12. [Validators](#12-validators)
13. [Preset Examples](#13-preset-examples)
14. [Core Executor Compatibility Checklist](#14-core-executor-compatibility-checklist) → [checklist-core.md](./checklist-core.md)
15. [Packaging: Validator / Executor Separation](#15-packaging-validator--executor-separation)
    - 15.1 [Rules](#151-rules)
    - 15.2 [Rationale](#152-rationale)
16. [Conformance Levels](#16-conformance-levels)
    - 16.1 [Omissions](#161-omissions)
    - 16.2 [Declaring the Level](#162-declaring-the-level)
    - 16.3 [Testkit Behaviour](#163-testkit-behaviour)
    - 16.4 [Outcome Labels](#164-outcome-labels)
    - 16.5 [What Partial Conformance Doesn't Mean](#165-what-partial-conformance-doesnt-mean)

---

## 1. Overview

Lace is a purpose-built scripting microlanguage for defining API monitoring probes. A probe is a `.lace` file — plain text that can be written by hand, stored in version control, and run directly:

```bash
lace run script.lace --vars vars.json
lace run script.lace --vars vars.json --prev-results last_result.json
lace validate script.lace
```

**The language is backend-agnostic.** The executor receives a flat variable map, runs the script, and returns a structured result. The backend acts on that result however it sees fit.

**Three executor implementations exist: Python, JavaScript, Kotlin.** They parse the same grammar, apply the same validation rules, and produce the same result structure for the same inputs.

**The executor returns results only — no side effects.** Notifications, logging, and any other output beyond the structured result are provided by the extension system.

**The core is minimal by design.** The `options {}` block on scopes and conditions is a core syntactic placeholder reserved for extension use. The core executor ignores it entirely.

---

## 2. Grammar

### 2.1 Formal Grammar (EBNF)

```ebnf
script          = call+ ;

call            = http_method "(" url_arg [ "," call_config ] ")"
                  chain_method+ ;

http_method     = "get" | "post" | "put" | "patch" | "delete" ;

url_arg         = string ;

call_config     = "{" call_field ( "," call_field )* [","] "}" ;

call_field      = "headers"   ":" object_lit
                | "body"      ":" body_value
                | "cookies"   ":" object_lit
                | "cookieJar" ":" string
                | "clearCookies" ":" "[" string ("," string)* [","] "]"
                | "redirects" ":" redirects_obj
                | "security"  ":" security_obj
                | "timeout"   ":" timeout_obj
                | IDENT       ":" expr ;    (* extension-registered call fields *)

body_value      = "json" "(" object_lit ")"
                | "form" "(" object_lit ")"
                | string ;

redirects_obj   = "{" redirects_field ("," redirects_field)* [","] "}" ;
redirects_field = "follow" ":" bool_lit
                | "max"    ":" integer_lit
                | IDENT    ":" expr ;

security_obj    = "{" security_field ("," security_field)* [","] "}" ;
security_field  = "rejectInvalidCerts" ":" bool_lit
                | IDENT ":" expr ;

timeout_obj     = "{" timeout_field ("," timeout_field)* [","] "}" ;
timeout_field   = "ms"      ":" integer_lit
                | "action"  ":" timeout_action
                | "retries" ":" integer_lit
                | IDENT     ":" expr ;    (* extension-registered timeout fields *)

timeout_action  = '"fail"' | '"warn"' | '"retry"' ;

chain_method    = expect_method
                | check_method
                | store_method
                | assert_method
                | wait_method ;

(* Fixed order: .expect → .check → .assert → .store → .wait
   Any subset valid. Each appears at most once. *)

expect_method   = ".expect" "(" scope_list ")" ;
check_method    = ".check"  "(" scope_list ")" ;

scope_list      = scope_entry ("," scope_entry)* [","] ;

scope_entry     = scope_name ":" scope_val ;

scope_name      = "status" | "body" | "headers" | "bodySize"
                | "totalDelayMs" | "dns" | "connect" | "tls"
                | "ttfb" | "transfer" | "size" ;

scope_val       = expr                    (* shorthand: value only, default op *)
                | "{" scope_obj_field ("," scope_obj_field)* [","] "}" ;

scope_obj_field = "value"   ":" expr
                | "op"      ":" op_key
                | "match"   ":" match_key      (* redirects scope only, §4.3 *)
                | "mode"    ":" mode_key       (* body: schema(...) only, §4.5.1 *)
                | "options" ":" options_obj ;

match_key       = '"first"' | '"last"' | '"any"' ;
mode_key        = '"loose"' | '"strict"' ;

(* options {} is a core placeholder for extensions.
   The core executor ignores all content. Extensions register
   fields into it via schema additions in their .laceext file. *)
options_obj     = "{" options_field ("," options_field)* [","] "}" | "{}" ;
options_field   = IDENT ":" expr ;       (* all fields extension-registered *)

op_key          = '"lt"' | '"lte"' | '"eq"' | '"neq"' | '"gte"' | '"gt"' ;

store_method    = ".store" "(" "{" store_entry ("," store_entry)* [","] "}" ")" ;
store_entry     = store_key ":" expr ;
store_key       = run_var | IDENT | string ;

assert_method   = ".assert" "(" "{" assert_body "}" ")" ;
assert_body     = assert_clause ("," assert_clause)* [","] ;
assert_clause   = ("expect" | "check") ":" "[" condition_item
                  ("," condition_item)* [","] "]" ;

condition_item  = expr
                | "{" cond_field ("," cond_field)* [","] "}" ;

cond_field      = "condition" ":" expr
                | "options"   ":" options_obj ;

wait_method     = ".wait" "(" integer_lit ")" ;

object_lit      = "{" object_entry ("," object_entry)* [","] "}" | "{}" ;
object_entry    = (string | IDENT) ":" expr ;

(*
 * Expression grammar is layered to make operator precedence explicit
 * and to forbid comparison-operator chaining. `a eq b eq c` is a parse
 * error; write `(a eq b) and (b eq c)` instead. Parentheses are the
 * only override for precedence.
 *
 * `and` and `or` use short-circuit evaluation: `false and f()` does
 * not evaluate `f()`; `true or f()` does not evaluate `f()`.
 *
 * Binary operators are left-associative: `1 - 2 - 3` = `(1 - 2) - 3`.
 *
 * Integer overflow is undefined. Executors should document their
 * integer representation. Portable scripts should stay within signed
 * 53-bit range (2^53 - 1) for cross-implementation correctness.
 *)
expr        = or_expr ;

or_expr     = and_expr ("or" and_expr)* ;
and_expr    = cmp_expr ("and" cmp_expr)* ;

(* Comparison is non-chaining: at most one operator per layer. *)
cmp_expr    = eq_expr ;
eq_expr     = ord_expr (("eq" | "neq") ord_expr)? ;
ord_expr    = addsub_expr (("lt" | "lte" | "gt" | "gte") addsub_expr)? ;

addsub_expr = muldiv_expr (("+" | "-") muldiv_expr)* ;
muldiv_expr = unary_expr  (("*" | "/" | "%") unary_expr)* ;

unary_expr  = "not" unary_expr
            | "-"   unary_expr
            | primary ;

primary     = "(" expr ")"
            | this_ref
            | prev_ref
            | script_var
            | run_var
            | literal
            | composite_lit
            | helper_call ;

composite_lit   = object_lit | array_lit ;
array_lit       = "[" expr ("," expr)* [","] "]" | "[]" ;

this_ref        = "this" ("." IDENT)+ ;
prev_ref        = "prev" ("." IDENT | "[" integer_lit "]")* ;

script_var      = "$" IDENT ("." IDENT | "[" integer_lit "]")* ;
run_var         = "$$" IDENT ("." IDENT | "[" integer_lit "]")* ;
literal         = string | integer_lit | float_lit | bool_lit | "null" ;

helper_call     = "json"   "(" object_lit ")"
                | "form"   "(" object_lit ")"
                | "schema" "(" script_var ")"
                | IDENT "(" [ expr ("," expr)* [","] ] ")" ;
(* Bare-IDENT calls are reserved for extension-registered helpers. The
   parser accepts any IDENT; the validator rejects unknown identifiers
   at §12 "Unknown expression function" when no active extension
   registered them. *)

size_string     = STRING matching /\d+(k|kb|m|mb|g|gb)?/i ;
```

### 2.2 Lexical Rules

```ebnf
script_var  = "$"  IDENT ;
run_var     = "$$" IDENT ;
IDENT       = [a-zA-Z_][a-zA-Z0-9_]* ;

string      = '"' string_char* '"' ;
string_char = any_char_except_dquote_and_backslash | escape_seq ;
escape_seq  = "\" ('"' | "\" | "n" | "t" | "r" | "$") ;

(* Variable interpolation ($var, $$var, ${$var}, ${$$var}) is a semantic
 * operation applied to string values during execution, not a lexer concern.
 * The lexer emits the entire string as a single token; the executor scans
 * the string body for interpolation references during evaluation. See §3.5
 * for the interpolation grammar applied to string contents. *)

integer_lit = [0-9]+ ;
float_lit   = [0-9]+ "." [0-9]+ ;
bool_lit    = "true" | "false" ;
comment     = "//" [^\n]* (newline | EOF) ;
(* Whitespace ignored between tokens *)
```

### 2.3 Structure Rules

Enforced by validator:

- Script contains at least one call
- Every call contains at least one chain method

  > **Rationale:** Lace is a probe language, not a request runner. A call with no chain methods makes an HTTP request and discards the result — providing no information about endpoint health, no assertions, no stored values. The one-chain-method minimum is what distinguishes a *probe* from a *request*.

- Chain methods appear in fixed order: `.expect → .check → .assert → .store → .wait`
- Each chain method appears at most once per call
- A `$$var` is assigned at most once across the entire script (§5.3)
- `clearCookies` only valid when `cookieJar` is a `selective_clear` variant
- `timeout.retries` requires `timeout.action: "retry"`
- `prev` reference valid only when `--prev-results` is provided; validator emits warning if used without it

---

## 3. HTTP Call Syntax

### 3.1 Methods & URL

```lace
get(url, config?)
post(url, config?)
put(url, config?)
patch(url, config?)
delete(url, config?)
```

`HEAD` is not supported — Lace probes require a response body.

### 3.2 Request Config

All fields optional. Extensions may register additional fields on the call config, `redirects`, `security`, and `timeout` objects via schema additions.

```lace
post("$BASE_URL/login", {
  headers:  { "X-Request-ID": "$request_id" },
  body:     json({ email: "$admin_email", password: "$admin_pass" }),
  cookies:  { locale: "en" },
  cookieJar: "named:admin",

  redirects: {
    follow: true,
    max: 10
  },

  security: {
    rejectInvalidCerts: false    // staging — self-signed cert
  },

  timeout: {
    ms:      5000,
    action:  "retry",
    retries: 2
    // extension fields (e.g. notification) registered here by laceNotifications
  }
})
```

**`redirects` defaults:** `follow: true`, `max: 10` (from `executor.maxRedirects`). Exceeding context system max for `max` is always a hard fail.

**`security` defaults:** `rejectInvalidCerts: true`. When `false`, TLS errors produce a warning in the call record and execution continues.

**`timeout` defaults:** `ms` from execution context default, `action: "fail"`, `retries: 0`.

**`timeout.action` values:**

| Value | Behaviour |
|---|---|
| `"fail"` | Hard fail on timeout |
| `"warn"` | Soft fail on timeout, continue |
| `"retry"` | Retry up to `retries` times, then hard fail. Notification (if configured via extension) fires on final failure. |

### 3.3 Cookie Jar

| Mode | Description |
|---|---|
| `"inherit"` | Continue previous call's jar. First call starts empty. |
| `"fresh"` | Discard all cookies, start empty. |
| `"selective_clear"` | Clear `clearCookies` entries from default jar, then continue. |
| `"named:{name}"` | Use or create isolated named jar. `{name}` non-empty alphanumeric. |
| `"{name}:selective_clear"` | Clear `clearCookies` from named jar, then continue. |

`"selective_clear"` without prefix = `"default:selective_clear"`. Named jars persist for run only.

### 3.4 Response Object (`this`)

Valid only within chain methods of the same call. Read-only.

| Field | Type | Description |
|---|---|---|
| `this.status` | integer | HTTP status code |
| `this.statusText` | string | HTTP status text |
| `this.body` | object \| string | Parsed JSON or raw string |
| `this.headers` | object | Response headers, lower-cased |
| `this.responseTime` | integer | Total response time ms |
| `this.connect` | integer | TCP connection ms |
| `this.ttfb` | integer | Time to first byte ms |
| `this.transfer` | integer | Body transfer ms |
| `this.size` | integer | Response body bytes |
| `this.redirects` | array of strings | Ordered URLs followed. Empty array if none. See §3.7. |
| `this.dns` | object | DNS resolution metadata — see §3.4.1. |
| `this.dnsMs` | integer | DNS resolution ms |
| `this.tls` | object \| null | TLS metadata; `null` for plain HTTP — see §3.4.2. |
| `this.tlsMs` | integer | TLS handshake ms (0 for HTTP) |

Scope shorthand: `.expect(dns: 100)` / `.expect(tls: 50)` reference the timing scalars (`dnsMs` / `tlsMs`). The object forms above are for script-level introspection (`this.dns.resolvedIp`, `this.tls.protocol`, …) and for extensions that want to operate on structured connection state.

#### 3.4.1 DNS Metadata

Executors **must** populate `this.dns` with the addresses resolved for the call's hostname:

```json
{
  "resolvedIps": ["93.184.216.34"],
  "resolvedIp":  "93.184.216.34"
}
```

- `resolvedIps` — every address returned by resolution, in the order the OS resolver gave them. Never omitted; at minimum a single-element array.
- `resolvedIp` — the address the executor actually connected to. Equal to `resolvedIps[0]` unless the executor iterated through addresses after a connect failure.

The executor surfaces these fields and nothing more. It performs no interpretation (no geolocation, no blocklist check) beyond populating the values — that's extension territory.

#### 3.4.2 TLS Metadata

For HTTPS calls, `this.tls` is an object:

```json
{
  "protocol": "TLSv1.3",
  "cipher":   "TLS_AES_256_GCM_SHA384",
  "alpn":     "h2",
  "certificate": {
    "subject":         { "cn": "example.com" },
    "subjectAltNames": ["DNS:example.com", "DNS:www.example.com"],
    "issuer":          { "cn": "R3" },
    "notBefore":       "2026-01-01T00:00:00Z",
    "notAfter":        "2026-04-01T00:00:00Z"
  }
}
```

- `protocol` — negotiated TLS protocol version, e.g. `"TLSv1.2"`, `"TLSv1.3"`.
- `cipher` — negotiated cipher suite name.
- `alpn` — negotiated ALPN protocol (e.g. `"h2"`, `"http/1.1"`) or `null` if no ALPN.
- `certificate` — peer certificate metadata or `null`. When the executor is configured with `rejectInvalidCerts: false`, some runtimes cannot expose the parsed certificate dictionary — they **must** still populate `protocol` / `cipher` / `alpn` and set `certificate: null` rather than omit the whole `tls` field. Fields inside `certificate`: `subject.cn`, `subjectAltNames` (array of `DNS:…` / `IP:…` tokens), `issuer.cn`, `notBefore` / `notAfter` (ISO-8601 UTC).

For plain HTTP, `this.tls` is `null`. The executor performs no interpretation of TLS metadata beyond surfacing it — certificate-pinning, cipher-allowlisting, and similar policy checks belong to extensions.

### 3.5 Variable Interpolation in Strings

| Syntax | Resolves to |
|---|---|
| `$varname` | Injected script variable |
| `$$varname` | Run-scope variable |

Disambiguation: `${$varname}`, `${$$varname}`. Use braced forms when a variable name is adjacent to other text (e.g. `${$host}name`). The `\$` escape sequence (§2.2) produces a literal `$` character but does not prevent interpolation — use braced forms for disambiguation instead. Missing variable → `null` (see §5.4).

Timestamps are available through the executor rather than the source language. The top-level result carries `startedAt` / `endedAt`; each call record carries its own `startedAt` / `endedAt`; extensions observing the `before script` hook see `script.startedAt` in context. Scripts that need a submit-time marker should inject one via `--var ts=...`.

### 3.6 Default Request Headers

Every executor **must** set a `User-Agent` header on outgoing requests so origin servers can attribute the traffic.

**Default value** (when neither script nor config overrides):

```
User-Agent: lace-probe/<executor-version> (<implementation-name>)
```

- `<executor-version>` is the executor implementation's own version string.
- `<implementation-name>` identifies the concrete executor (e.g. `lacelang-python`, `lacelang-js`, `lacelang-kotlin`, `lacelang-c`). Any RFC 7231-compliant UA is acceptable.

**Overrides**, precedence highest to lowest:

1. Script `headers: { "User-Agent": "..." }` on the call — per-request.
2. `lace.config [executor].user_agent = "..."` — per-deployment. A host platform sets this to its own fleet identifier (e.g. a monitoring service may pick `<service-name>/<build-id>`). The executor uses the configured string verbatim.
3. The default above.

Executors should set no other implicit headers beyond what the HTTP stack requires (`Host`, `Content-Length`, `Content-Type` when a body is attached via `json()`/`form()`, etc).

### 3.7 Redirect Tracking

Every executor **must** record the ordered list of URLs followed during a call on `calls[n].redirects` in the result and on `this.redirects` inside the chain.

- The list contains the resolved URL of each redirect hop **that was actually issued**, in order. It does **not** include the initial request URL (already available as `calls[n].request.url`), and it does **not** include the final response's URL if the final response was not itself a redirect.
- Empty array `[]` when the call issued no redirects (including when `redirects.follow = false`).
- Populated even when the call hard-fails due to `REDIRECTS_MAX_LIMIT` — the list shows the hops up to and including the one that triggered the limit.

Example: `get("/a")` receiving `302 → /b`, then `302 → /c`, then `200` at `/c` produces `redirects: ["/b", "/c"]`.

Redirect hops may be asserted via the `redirects` scope (§4.3) with a `match` of `first`, `last`, or `any`.

---

## 4. Chain Methods

### 4.1 `.expect()`

Validates response properties. **All scopes are evaluated before any failure triggers a hard fail.** This means all failing scopes are collected and recorded together — the author sees every problem at once, not just the first.

After all scopes are evaluated: if any scope failed, execution hard-fails (subsequent chain methods on this call and all subsequent calls are skipped).

```lace
.expect(
  // shorthand — value only, default op, no options
  dnsMs: 200,

  // value with op override
  totalDelayMs: { value: 500, op: "lte" },

  // full form — value, op, and options block for extensions
  status: {
    value: 200,
    options: {
      // fields registered by extensions (e.g. laceNotifications)
    }
  }
)
```

At least one scope must be present.

### 4.2 `.check()`

Validates response properties as soft checks. **All scopes are evaluated** — same as `.expect()`. After all scopes are evaluated: if any scope failed, a failure is recorded and execution continues to the next chain method.

`.check()` accepts exactly the same scope syntax as `.expect()`. The only difference is that failures are soft, not hard.

```lace
.expect(totalDelayMs: { value: 10000 })   // hard fail over 10s
.check(totalDelayMs: { value: 1000 })     // soft fail over 1s
```

### 4.3 Scope Form Reference

Every scope in `.expect()` and `.check()` accepts three forms:

**Shorthand** — bare value, default op inferred from scope name (§4.4), no options:
```lace
totalDelayMs: 200
status: 200
status: [200, 201]
```

**Value + op** — explicit comparison operator, no options:
```lace
totalDelayMs: { value: 500, op: "lte" }
```

**Full form** — value, optional op, and options block:
```lace
status: {
  value: 200,
  op: "eq",          // optional — default inferred if omitted
  options: { ... }   // core placeholder, ignored by core executor
}
```

All three forms produce identical core behaviour. The `options` block is available to extensions in the full form.

**Available scopes:**

| Scope | Type | Description |
|---|---|---|
| `status` | integer \| integer[] | HTTP status code. Array = any match passes. |
| `body` | body match | See §4.5. |
| `headers` | object | Each key-value must match. Keys case-insensitive. |
| `bodySize` | size string | Body size threshold: `50`, `50k`, `50kb`, `10m`, `10mb`, `1g`, `1gb`. Default op `lt`. Also gates body capture — a response whose body exceeds this value is not captured to `bodyPath` and the call records `bodyNotCapturedReason: "bodyTooLarge"`. |
| `totalDelayMs` | integer | `this.responseTime` threshold in ms. |
| `dns` | integer | `this.dns` threshold in ms. |
| `connect` | integer | `this.connect` threshold in ms. |
| `tls` | integer | `this.tls` threshold in ms. Skipped when `this.tls eq 0`. |
| `ttfb` | integer | `this.ttfb` threshold in ms. |
| `transfer` | integer | `this.transfer` threshold in ms. |
| `size` | integer | Exact `this.size` in bytes (default op `eq`). Use `bodySize` for threshold checks. |
| `redirects` | string | Assertion over `this.redirects` (spec §3.7). Uses a `match` field — see below. |

**`redirects` scope form.** Unlike other scopes, `redirects` takes an extra `match` field choosing which redirect URL to compare:

```lace
// Shorthand — defaults to match: "any"
redirects: "/login"

// Full form — explicit match type
redirects: { value: "/login",              match: "first" }
redirects: { value: "/admin",              match: "any"   }
redirects: { value: "https://final.com/",  match: "last"  }
```

| `match` | Behaviour |
|---|---|
| `first` | Compare `value` to `this.redirects[0]`. Fails if the array is empty. |
| `last`  | Compare `value` to `this.redirects[-1]`. Fails if the array is empty. |
| `any`   | Pass if `value` equals any element of `this.redirects`. |

Default `match`: `any`. Default `op`: `eq` (the only op `redirects` accepts today; ordered comparisons are not meaningful for URLs).

### 4.4 Default Operators

When `op` is omitted, the executor uses the scope's natural default:

| Scope | Default op | Reasoning |
|---|---|---|
| `status` | `eq` | Status codes are exact matches |
| `body` | `eq` | Exact or schema match |
| `headers` | `eq` | Each header matched exactly |
| `size` | `eq` | Exact byte count |
| `redirects` | `eq` | URL equality (with `match` selecting which hop, §4.3) |
| `totalDelayMs` | `lt` | "less than threshold" |
| `dns` | `lt` | Threshold check |
| `connect` | `lt` | Threshold check |
| `tls` | `lt` | Threshold check |
| `ttfb` | `lt` | Threshold check |
| `transfer` | `lt` | Threshold check |
| `bodySize` | `lt` | Threshold check |

### 4.5 Body Matching

| Form | Example | Behaviour |
|---|---|---|
| Schema validation | `body: schema($user_schema)` | Validates response JSON against JSON Schema in variable (§4.5.1). |
| Literal string | `body: "OK"` | Raw body must equal this exactly. |
| Variable match | `body: $expected` | Raw body must equal runtime value of variable. |

`schema($var)` where `$var` is null → hard fail.

#### 4.5.1 Schema Match Modes

Schema-based body matching supports a `mode` modifier controlling whether extra fields on the response body cause the assertion to fail.

```lace
// Default — `loose`: all fields declared in the schema must be present and
// valid; additional fields are allowed.
body: schema($user_schema)

// Equivalent explicit form.
body: { value: schema($user_schema), mode: "loose" }

// `strict`: every response field must be declared by the schema, AND every
// schema field must be present. No extra fields. Equivalent to a JSON Schema
// with `additionalProperties: false` at every level.
body: { value: schema($user_schema), mode: "strict" }
```

| `mode` | Behaviour |
|---|---|
| `loose` (default) | Schema fields all present and type-valid; extras accepted. |
| `strict` | Schema fields all present and type-valid; **no** extra fields at any level. |

The `mode` field applies **only** to `body: schema(...)` forms. Literal-string and variable-match body forms are always byte-exact; `mode` on those is silently ignored.

When a `schema($var)` body check fails, the assertion record carries the first validation error path and message in `actual` (e.g. `{ path: ".user.id", detail: "expected integer, got string" }`). Strict-mode extra-field failures use path `.<field>` and detail `"unexpected field"`.

### 4.6 `.store()`

```lace
.store({
  "$$token":  this.body.access_token,   // run-scope, write-once
  "$cursor":  this.body.next_cursor,    // write-back → actions.variables
  last_count: this.body.count           // write-back → actions.variables (same as $cursor)
})
```

| Key | Behaviour |
|---|---|
| `$$name` | Run-scope. Write-once (§5.3). In `runVars`. Never in `actions.variables`. |
| `$name` | Write-back. In `actions.variables`. Backend decides persistence. The `$` is stripped from the key in the result. The variable's in-memory value does not change during this run (§5.1). |
| Plain key | Write-back. Equivalent to `$name`. In `actions.variables`. Backend decides scope and persistence. |

Values may be any JSON-serialisable shape: string, integer, float, boolean, null, object, or array. Null is valid (explicit clear). Structured values (e.g. `$$user_schema = this.body`) are allowed so scripts can capture schemas or subtrees for use in later calls and by extensions.

`.store()` is skipped if any preceding chain method produced a hard fail on this call — that is, if `.expect()` or `.assert({ expect: [...] })` failed. This is a consequence of the general hard-fail cascade rule (§7) and the chain order (`.expect → .check → .assert → .store → .wait`).

### 4.7 `.assert()`

Custom expression conditions. Hard (`expect`) or soft (`check`). The `options` block follows the same core-placeholder pattern as scope options.

```lace
.assert({
  expect: [
    // shorthand — no options
    this.body.success eq true,

    // full form
    {
      condition: $$count_after eq $$count_before + 1,
      options: {
        // extension fields registered here (e.g. silentOnRepeat, notification)
      }
    }
  ],
  check: [
    {
      condition: this.responseTime lt $sla_ms,
      options: {}
    }
  ]
})
```

All `expect` conditions are evaluated before triggering a hard fail — same complete-evaluation model as `.expect()`. All `check` conditions are always evaluated.

**Expressions:** arithmetic (`+`, `-`, `*`, `/`, `%`), comparison keywords (`eq`, `neq`, `lt`, `lte`, `gt`, `gte`), logical connectives (`and`, `or`, `not`), null check (`eq null`, `neq null`). Comparisons do not chain — `a eq b eq c` is a parse error; compose with `(a eq b) and (b eq c)` instead. Parentheses are the only way to override precedence. `this.*`, `prev.*`, `$var`, `$$var`, and `json()`/`form()`/`schema()` are valid. Null operands in ordered comparison or arithmetic → indeterminate (§5.4).

### 4.8 `.wait()`

Pauses after all other chain methods complete. Always the last method.

```lace
.wait(2000)
```

Lace has no wait-duration ceiling — recurrence and pacing concerns belong to the host platform, not the core language.

---

## 5. Variables

### 5.1 Script Variables (`$var`)

Injected by the backend as a flat plaintext key-value map before execution. The language has no concept of scope, storage, or access rules.

Script variables are read-only during execution — their in-memory value never changes. However, `$var` keys may appear in `.store()` as write-back targets: the value goes to `actions.variables`, signalling the backend to update the variable for future runs. The current run still reads the originally injected value.

**Schema variables** are script variables whose injected value is a valid JSON Schema document string. Used only in `schema($var)`.

### 5.2 Run-Scope Variables (`$$var`)

Created by `.store()` with `$$` prefix. Available to all subsequent chain methods and calls in this run. Returned in `runVars` for debugging and backend use.

### 5.3 Immutability Rule

A `$$var` may be assigned **exactly once** across the entire script. A second assignment to the same `$$` key is a **validation error**.

`runVars` in the result is therefore unambiguous — each key has exactly one value from exactly one assignment point.

### 5.4 Null Semantics

- `$var` absent from the injected map → `null`
- `$$var` not yet assigned → `null`
- `prev.*` when no previous results provided → `null`
- `null` in string interpolation → literal string `"null"` + warning recorded
- `null eq null` → `true`. `null eq value` → `false`. `null neq value` → `true`
- `null` as operand of `lt`, `gt`, `lte`, `gte`, `+`, `-`, `*`, `/`, `%` → **indeterminate**: condition recorded as `"indeterminate"`, no error, execution continues
- `schema($var)` where `$var` is `null` → hard fail

### 5.5 Injection Model

The backend resolves its own scoping and access rules then injects a flat plaintext map. The executor receives only resolved plaintext values.

CLI: `--vars vars.json` (JSON object) or `--var KEY=VALUE`. Multiple `--var` flags merge.

---

## 6. Previous Results (`prev`)

When `--prev-results path` is provided (or `prev_results` in config), the previous result JSON is loaded and made accessible via the `prev` reference in expressions.

**Access syntax** mirrors the result structure (§9):

```lace
prev.outcome                                  // "success" | "failure" | "timeout" | null
prev.runVars.token                           // previous $$token value
prev.calls[0].outcome                         // previous first call outcome
prev.calls[0].response.status                 // previous first call status code
prev.calls[0].assertions[0].outcome           // previous first assertion outcome
prev.calls[0].assertions[0].actualLhs        // previous assertion left operand
```

`prev` resolves to `null` when no previous results are provided. Any field access on `null` propagates `null` (does not throw). Null propagation follows §5.4.

**`prev` is a core feature.** It is available to all expressions regardless of which extensions are active. Extensions (e.g. `laceNotifications`) use it for `silentOnRepeat` logic, but it is not owned by any extension.

---

## 7. Failure Semantics

| Type | Source | Behaviour |
|---|---|---|
| **Hard fail** | `.expect()` — after all scopes evaluated, ≥1 failed | Stop: skip remaining chain methods on this call and all subsequent calls |
| **Hard fail** | `.assert({ expect })` — after all conditions evaluated, ≥1 failed | Stop as above |
| **Hard fail** | `redirects.max` exceeded | Stop — not overridable |
| **Hard fail** | `security.rejectInvalidCerts: true` + TLS error | Stop — not overridable |
| **Hard fail** | `schema($var)` where var is null | Stop |
| **Hard fail** | `timeout.action: "fail"` | Stop |
| **Hard fail** | All retries exhausted (`timeout.action: "retry"`) | Stop — extension handles notification via `on call` hook |
| **Soft fail** | `.check()` — after all scopes evaluated, ≥1 failed | Record all failures, continue |
| **Soft fail** | `.assert({ check })` — after all conditions evaluated, ≥1 failed | Record all failures, continue |
| **Soft fail** | `timeout.action: "warn"` | Record, continue |
| **Soft fail** | `security.rejectInvalidCerts: false` + TLS error | Record warning, continue |
| **Indeterminate** | Null operand in ordered comparison or arithmetic | Record as `"indeterminate"`, continue |

**Complete evaluation before cascade:** `.expect()` evaluates all scopes. `.assert({ expect })` evaluates all conditions. Only after full evaluation does the hard-fail cascade trigger. This ensures all failures are visible simultaneously.

**`.store()` skip:** skipped when any preceding chain method on the same call hard-failed — `.expect()` failure or `.assert({ expect: [...] })` failure.

---

## 8. Helper Functions

Valid in expressions only:

| Function | Signature | Description |
|---|---|---|
| `json({...})` | `(object) → string` | Serialise to JSON. Variable interpolation first. |
| `form({...})` | `(object) → string` | Serialise to URL-encoded form. |
| `schema($var)` | `(script_var) → schema_ref` | Body schema validation reference. |

---

## 9. Executor Result

### 9.1 Result Structure

```json
{
  "outcome":   "success",
  "startedAt": "2024-01-15T14:23:01.234Z",
  "endedAt":   "2024-01-15T14:23:02.891Z",
  "elapsedMs": 1657,
  "runVars": {
    "token":       "abc123",
    "countBefore": 41,
    "countAfter":  42
  },
  "calls":   [ ...call records... ],
  "actions": {
    "variables": { "lastCount": 42 }
  }
}
```

| Field | Description |
|---|---|
| `outcome` | `"success"` \| `"failure"` \| `"timeout"` |
| `startedAt` | UTC ISO 8601 timestamp before the first call begins. |
| `endedAt` | UTC ISO 8601 timestamp after all chain methods complete or after cascade stops. |
| `elapsedMs` | Wall-clock elapsed time in milliseconds. |
| `runVars` | Final state of all `$$vars`. Each key appears exactly once. |
| `calls` | Ordered call records including skipped calls. |
| `actions` | Free-form. `actions.variables` is the only typed mandatory section. Extensions add further fields. |

### 9.2 Call Record

```json
{
  "index":     0,
  "outcome":   "success",
  "startedAt": "2024-01-15T14:23:01.234Z",
  "endedAt":   "2024-01-15T14:23:01.379Z",
  "request": {
    "url":      "https://api.example.com/login",
    "method":   "post",
    "headers":  { "content-type": "application/json" },
    "bodyPath": "/probe_runs/abc/call_0_request.json"
  },
  "response": {
    "status":         200,
    "statusText":     "OK",
    "headers":        { "content-type": "application/json" },
    "bodyPath":       "/probe_runs/abc/call_0_response.json",
    "responseTimeMs": 145,
    "dnsMs":          12,
    "connectMs":      34,
    "tlsMs":          28,
    "ttfbMs":         98,
    "transferMs":     47,
    "sizeBytes":      1024
  },
  "redirects": [],
  "assertions": [
    {
      "method":   "expect",
      "scope":    "status",
      "op":       "eq",
      "outcome":  "passed",
      "actual":   200,
      "expected": 200,
      "options":  { }
    },
    {
      "method":     "assert",
      "kind":       "expect",
      "index":      0,
      "outcome":    "failed",
      "expression": "$$countAfter eq $$countBefore + 1",
      "actualLhs":  42,
      "actualRhs":  41,
      "options":    { }
    }
  ],
  "config": {
    "timeout":   { "ms": 5000, "action": "fail", "retries": 0 },
    "redirects": { "follow": true, "max": 10 },
    "security":  { "rejectInvalidCerts": true }
  },
  "warnings": [],
  "error":    null
}
```

| Field | Description |
|---|---|
| `outcome` | `"success"` \| `"failure"` \| `"timeout"` \| `"skipped"` |
| `redirects` | Ordered URLs followed — see §3.7. Empty array when no redirects. |
| `assertions[].outcome` | `"passed"` \| `"failed"` \| `"indeterminate"` |
| `assertions[].options` | The `options {}` object from the source, passed through opaquely. Extensions read this. |
| `config` | Resolved call config (after defaults applied). Extensions may read registered fields here. |
| `request.bodyPath` | Absolute path to request body file. `null` if no body. |
| `response` | `null` for timeout, skipped, or connection failure. |
| `response.bodyPath` | Absolute path to response body file. `null` if not captured. |
| `response.bodyNotCapturedReason` | `"bodyTooLarge"` \| `"notRequested"` \| `"timeout"`. Present when `bodyPath` is null. |
| `warnings` | Warning strings from this call (null interpolations, TLS warnings, skipped writes). |
| `error` | Non-assertion failure detail. `null` otherwise. |

### 9.3 Actions

**`actions.variables`** — present when any write-back `.store()` targets exist:

```json
{ "last_count": 42, "cursor": "abc123" }
```

Flat map. For `$name` keys, the `$` prefix is stripped (e.g. `"$cursor"` in source → `"cursor"` in result). Plain keys appear as-is. Values may be any JSON-serialisable shape.

All other `actions` fields are extension-defined (see §10).

### 9.4 Body Storage

Executor writes request and response bodies to a shared filesystem volume. Result JSON contains absolute paths — no body bytes in the result JSON itself.

Path convention: `{run_base_dir}/call_{index}_{request|response}.{ext}`

Run base directory provided in execution context (configured via `result.bodies.dir` in `lace.config`).

---

## 10. Extension System (Overview)

Lace has a declarative extension system. Extensions are `.laceext` files that add schema fields, result fields, and rules. The full specification of the extension file format, rule body language, hook points, and compatibility requirements is in **[lace-extensions.md](./lace-extensions.md)**.

**What the core executor guarantees for extensions:**

- The `options {}` block on every scope in `.expect()`/`.check()` and on every condition in `.assert()` is a core syntactic placeholder. Its content is extension-defined. The core executor passes it through to the result opaquely in `assertions[].options`.
- Extension-registered fields on call config sub-objects (`timeout`, `redirects`, `security`, call root) are passed through to `calls[n].config` in the result.
- Five hook points fire during execution: `on before expect`, `on before check`, `on before store`, `on before assert`, `on before call`, and their post-execution counterparts `on expect`, `on check`, `on store`, `on assert`, `on call`. Extensions register rules against these hooks.
- Extensions may write to `result.runVars` only under their own namespace: keys must be prefixed `{extension_name}.`. Extension variables in `runVars` are not readable from `.lace` scripts — they are for extension internal state and backend consumption only.
- Unknown extension fields in a script when the extension is not active produce warnings (if `laceLogging` is active), not errors, unless they break core syntactic structure.

## 11. Configuration (`lace.config`)

A TOML file. All values have defaults — the config file is entirely optional.

```toml
[executor]
# Extensions to activate (must have corresponding .laceext files)
extensions = []

# System limits passed to execution context
maxRedirects = 10
maxTimeoutMs = 300000

[result]
# Where to save the result JSON.
# Directory: saves {dir}/{YYYY-MM-DD_HH-MM-SS}.json (sortable, no collisions)
# Full path: always overwrites that file
# false: do not save
path = "./lace_results"

[result.bodies]
# Where to write request/response body files
dir = "./lace_results/bodies"

[extensions.laceNotifications]
# Path to .laceext file (default: bundled with executor)
laceext = "builtin:laceNotifications"

# Previous results file for silentOnRepeat evaluation
# Overridden by --prev-results flag
# Omit this key to leave it unset (the extension sees null for the field)
prev_results = false

[extensions.laceLogging]
laceext = "builtin:laceLogging"
level   = "warn"           # "info" | "warn" | "error"
include_in_result = true
stdout = false

[extensions.myCustomExtension]
laceext = "./extensions/myExtension.laceext"
# Extension-specific config fields (free-form, read by extension rules)
api_key = "env:MY_EXT_API_KEY"
api_key_with_default = "env:MY_EXT_API_KEY:fallback_value"
```

Extensions may ship a companion `{extName}.config` file alongside their `.laceext` file, declaring default values for their config fields (see `lace-extensions.md §2.3`). When both files exist, the `.config` defaults serve as the base and `lace.config` overrides take precedence for any key present in both. Keys absent from `lace.config` retain the extension's declared defaults.

**All config field defaults:**

| Field | Default | Description |
|---|---|---|
| `executor.extensions` | `[]` | No extensions active |
| `executor.maxRedirects` | `10` | System redirect limit |
| `executor.maxTimeoutMs` | `300000` | System timeout limit (5 minutes) |
| `result.path` | `"."` | Current directory |
| `result.bodies.dir` | Same as `result.path` | Body storage directory |

**Environment variable syntax:** any string config value may reference an environment variable:
- `"env:VARNAME"` — resolves to the value of `VARNAME`, error at startup if unset
- `"env:VARNAME:default"` — resolves to `VARNAME` if set, `default` otherwise

**Config resolution order** (highest priority first):
1. CLI flags (`--vars`, `--prev-results`, `--save-to`, `--config`)
2. `lace.config` in script directory
3. `lace.config` in working directory
4. Built-in defaults

**`--save-to` flag:** overrides `result.path` for a single run.

**`lace.config.{env}` usage:** the `{env}` suffix is also supported in config section names for environment-specific config:

```toml
[lace.config.production]
executor.maxTimeoutMs = 10000

[lace.config.staging]
executor.maxTimeoutMs = 30000
```

Active environment selected via `LACE_ENV` environment variable or `--env` flag.

---

## 12. Validators

Two stages: **language validator** (all implementations) and **platform validator** (backend-only).

Errors prevent enabling. Warnings allow saving and enabling.

| Rule | Severity | Description |
|---|---|---|
| Syntax | **Error** | Source text parses against §2 grammar |
| At least one call | **Error** | ≥ 1 HTTP call |
| Call completeness | **Error** | Every call has ≥ 1 chain method |
| Chain method order | **Error** | `.expect → .check → .assert → .store → .wait` |
| Method at most once | **Error** | Each method appears at most once per call |
| `.expect()` / `.check()` non-empty | **Error** | Method with zero scopes |
| `this.*` scope | **Error** | `this` referenced outside a chain method body (a parser-level invariant ensures each call's chain is its own scope, so cross-call references are not constructible) |
| Unknown expression function | **Error** | Not in `json`, `form`, `schema` |
| Variable existence | **Error** | All `$var` references in registry |
| `$$var` write-once | **Error** | Same `$$` key assigned > once |
| `schema()` argument | **Error** | Variable must exist in registry |
| Expression syntax | **Error** | `.assert()` expressions parse without error; also emitted for `.wait()` when its argument is not an integer literal |
| `redirects.max` limit | **Error** | Value > context system max |
| `timeout.ms` limit | **Error** | Value > context system max |
| `timeout.retries` requires action | **Error** | `retries` without `timeout.action: "retry"` |
| `clearCookies` jar mode | **Error** | `clearCookies` without `selective_clear` mode |
| `named:` empty name | **Error** | Empty name in `"named:"` jar mode |
| `op` value | **Error** | `op` not in: `lt`, `lte`, `eq`, `neq`, `gte`, `gt` |
| `bodySize` format | **Error** | Invalid size string |
| Unknown field (extension inactive) | **Warning** | Extension-registered field present, extension not active |
| `prev` without `--prev-results` | **Warning** | `prev.*` reference without previous results |
| High call count | **Warning** | > 10 calls |
| Platform chain length `[Platform]` | **Error — blocks enabling** | Platform-specific, not a language rule |

---

## 13. Preset Examples

### Minimal uptime check

```lace
get("$BASE_URL/health")
.expect(status: 200)
.check(totalDelayMs: 500)
```

### Full timing breakdown

```lace
get("$BASE_URL/api/search", {
  headers: { Authorization: "Bearer $api_key" }
})
.expect(
  status: 200,
  ttfb: { value: 300 },
  bodySize: "128kb"
)
.check(
  totalDelayMs: { value: 1200 },
  dns:        { value: 80 },
  connect:    { value: 150 },
  tls:        { value: 200 },
  transfer:   { value: 400 }
)
```

### With laceNotifications

```lace
get("$BASE_URL/api/search", {
  timeout: {
    ms: 5000,
    action: "retry",
    retries: 2,
    notification: template("search_timeout")   // registered by laceNotifications
  }
})
.expect(
  status: {
    value: 200,
    options: {
      silentOnRepeat: true,
      notification: {
        "neq":  template("wrong_status"),
        "404":  template("not_found"),
        "default": template("unexpected_status")
      }
    }
  },
  totalDelayMs: {
    value: 3000,
    options: {
      notification: template("too_slow")
    }
  },
  dnsMs: 100    // shorthand — extension generates default error text on failure
)
.check(
  ttfb: {
    value: 200,
    options: {
      silentOnRepeat: true,
      notification: { "gte": template("ttfb_degraded") }
    }
  }
)
.assert({
  expect: [{
    condition: $$count_after eq $$count_before + 1,
    options: {
      silentOnRepeat: false,
      notification: {
        "lt":     template("count_decreased"),
        "eq":     template("count_unchanged"),
        "default": template("count_wrong")
      }
    }
  }]
})
```

### Auth chain with cookie isolation

```lace
post("$BASE_URL/admin/login", {
  body:      json({ email: "$admin_email", password: "$admin_pass" }),
  cookieJar: "named:admin",
  timeout:   { ms: 3000, action: "fail" }
})
.expect(status: 200)
.store({ "$$admin_token": this.body.token })

get("$BASE_URL/admin/dashboard", {
  headers:   { Authorization: "Bearer $$admin_token" },
  cookieJar: "named:admin"
})
.expect(
  status: 200,
  body:   schema($dashboard_schema)
)
.check(totalDelayMs: { value: 500 })

delete("$BASE_URL/admin/resources/$$resource_id", {
  headers:   { Authorization: "Bearer $$admin_token" },
  cookieJar: "named:admin"
})
.expect(status: 200)
```

### Metric increment with write-once vars

```lace
get("$BASE_URL/metrics/events", {
  headers: { Authorization: "Bearer $metrics_key" }
})
.expect(status: 200)
.store({ "$$count_before": this.body.count })

post("$BASE_URL/events", {
  headers: { Authorization: "Bearer $api_key" },
  body:    json({ type: "probe_test" })
})
.expect(status: [200, 201, 202])
.check(totalDelayMs: { value: 1000 })
.wait(2000)

get("$BASE_URL/metrics/events", {
  headers: { Authorization: "Bearer $metrics_key" }
})
.expect(status: 200)
.store({
  "$$count_after": this.body.count,
  last_event_count: this.body.count
})
.assert({
  expect: [{
    condition: $$count_after eq $$count_before + 1,
    options: {
      silentOnRepeat: false,
      notification: {
        "lt":     template("event_count_decreased"),
        "eq":     template("event_count_unchanged"),
        "default": template("event_count_unexpected")
      }
    }
  }]
})
```

---

---

## 14. Core Executor Compatibility Checklist

Moved to **[checklist-core.md](./checklist-core.md)** for maintainability. The checklist covers parsing, validation, variable handling, null semantics, HTTP execution, cookie jar, chain method execution, body matching, prev access, failure cascade, result structure, body storage, configuration, and the core-side extension interface.

---

## 15. Packaging: Validator / Executor Separation

Every Lace implementation **must** ship its language validator and its runtime executor as **two distinct, independently-installable packages**. One must not force-install the other.

| Package role | Provides | Depends on | Typical consumers |
|---|---|---|---|
| **Validator** | Lexer, parser, semantic checks, canonical error codes, CLI with `parse` and `validate` subcommands | nothing (zero network surface) | CI jobs, IDE / editor plugins, script-authoring tools, backend platform validators |
| **Executor** | Runtime (HTTP client, assertion evaluation, cookie jars, extension dispatch, body storage), CLI with `parse` / `validate` / `run` subcommands | the validator package | probe runners, monitoring fleets, any platform hosting Lace |

### 15.1 Rules

- The validator package **must not** link an HTTP client, TLS stack, DNS resolver, cookie library, notification dispatcher, or any network-capable dependency. Installing it must be safe in sandboxed environments (read-only filesystems, no egress, air-gapped CI).
- The executor package **must** depend on the validator package at the same spec-compatible version and delegate `parse` + `validate` subcommands to it. It must not re-implement parsing or validation logic.
- Both packages share the same spec version.
- Both may share a monorepo or live in separate repositories. The choice is an implementation detail; the **package** separation is mandatory.
- The executor's CLI **must** expose all three conformance subcommands (`parse`, `validate`, `run`) so the testkit harness can drive it end-to-end with a single `-c <cmd>` flag.
- The validator's CLI **must** expose `parse` and `validate`, and **must not** expose `run`.
- Package naming: the spec does not mandate a convention, but the canonical suggestion is `lacelang-validator-<lang>` and `lacelang-executor-<lang>` (e.g. `lacelang-validator` / `lacelang-executor` on PyPI, `@lacelang/validator` / `@lacelang/executor` on npm).

### 15.2 Rationale

Most consumers of `.lace` source text do not run probes. IDE linters, CI gates, and backend platform validators need syntax and semantic checks but have no reason to depend on an HTTP stack. Forcing the runtime into every dependency tree enlarges the supply-chain surface and excludes constrained environments. Conversely, some runners may want to validate, then hand off execution to a remote worker — that worker, too, should be able to install the validator alone.

The split also makes conformance auditing easier: a validator package is a pure function from text to diagnostics, so its correctness is tractably unit-testable without network mocks or subprocess sandboxing.

---

---

## 16. Conformance Levels

Not every executor implementation needs every feature of the spec. Some targets — embedded probe runners, CI gates, bespoke monitoring appliances — have no use for the extension system or no surface for emitting writeback actions. Lace defines **conformance levels** so such implementations can declare what they support and have that claim verified by the testkit.

### 16.1 Omissions

Two optional omissions may be declared:

| Omission | Meaning |
|---|---|
| `extensions` | The executor does not implement the `.laceext` processor. `.laceext` files are never loaded, no hook dispatch occurs, `require` is not enforced, and the `EXT_*` error codes are not emitted. |
| `actions` | The executor does not emit the `result.actions` object. `.store()` with `$$name` keys still works (lands in `runVars`); `$name` and plain-key writebacks have no observable side effect. The rest of the result structure is unchanged. |

No other axis is omissible at the spec level. TLS handshake scenarios, cookie jar modes, body storage, prev-access, and every other core feature are mandatory for any non-omitted level.

### 16.2 Declaring the Level

**In the executor manifest** (`lace-executor.toml`):

```toml
[conformance]
omit = ["extensions"]         # or ["actions"], or both, or [] for full
```

Absent or empty `omit` means full conformance.

**On the testkit CLI** (overrides the manifest when present):

```bash
lace-conformance -m ./lace-executor.toml --omit extensions,actions
```

### 16.3 Testkit Behaviour

Each conformance vector carries an optional `requires` array listing the features it exercises:

```json
{ "id": "writeback_appears_in_actions_variables",
  "type": "execute",
  "requires": ["actions"],
  ... }
```

A vector whose `requires` list intersects the active omit set is **skipped** (outcome: `skipped`, reason: `"omitted: <feature>"`). Vectors without a `requires` list are always run.

### 16.4 Outcome Labels

The testkit reports a **conformance label** at the end of each run:

| Situation | Label |
|---|---|
| No `omit`; every runnable vector passed | `compliant` |
| Non-empty `omit`; every runnable vector passed | `compliant-partial (omit: <features>)` |
| Any level; ≥1 in-scope failure | `non-compliant` |

Process exit code: `0` for `compliant` and `compliant-partial`, `1` for `non-compliant`. A CI gate that requires full compliance enforces that separately (e.g. `--require-level=full`, to be added when actually needed).

### 16.5 What Partial Conformance *Doesn't* Mean

- A `compliant-partial` executor is **not** spec-incompatible. It is spec-compliant at its declared level. Host platforms that don't use extensions can legitimately claim `compliant-partial (omit: extensions)` with no stigma.
- Omitting a feature removes both the obligation to implement it and the right to emit its artefacts. An executor claiming `omit: actions` must not emit a `result.actions` field in some runs and not others — it is a structural guarantee, not a runtime toggle.
- `omit` does **not** affect the CLI contract: `parse` / `validate` / `run` subcommands, exit codes, stdout shape, and timeout handling are identical across all levels.

---

*End of Lace specification v0.9.0*
