# Grammar Reference

Lace v0.9.7<!-- sv --> grammar. The ANTLR4 grammar (`lacelang.g4`) is the authoritative syntax definition. The EBNF below is the human-readable mirror from the spec.

## Formal Grammar (EBNF)

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

(* Fixed order: .expect -> .check -> .assert -> .store -> .wait
   Any subset valid. Each appears at most once. *)

expect_method   = ".expect" "(" scope_list ")" ;
check_method    = ".check"  "(" scope_list ")" ;

scope_list      = scope_entry ("," scope_entry)* [","] ;

scope_entry     = scope_name ":" scope_val ;

scope_name      = "status" | "body" | "headers" | "bodySize"
                | "totalDelayMs" | "dns" | "connect" | "tls"
                | "ttfb" | "transfer" | "size" | "redirects" ;

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
store_key       = run_var | script_var | IDENT | string ;

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
(* Bare-IDENT calls cover extension-registered helpers and the two core
   assert-only functions `count` / `includes` (§8.1). The parser accepts any
   IDENT; the validator rejects unknown identifiers at §12 "Unknown expression
   function" when no active extension registered them and they are not a
   `count`/`includes` call inside an `.assert()` condition. *)

size_string     = STRING matching /\d+(k|kb|m|mb|g|gb)?/i ;

(* Notes on the EBNF vs. the ANTLR grammar (lacelang.g4, the authoritative
   syntax -- see notes/grammar.md):
   - IDENT in object-key and path positions (object_entry, this_ref,
     prev_ref, script_var, run_var, store_key) also admits keyword-shaped
     words: `this.body`, `{ status: 1 }`, `.store({ size: ... })` are valid.
   - Extension-registered field values (the IDENT ":" expr fallthroughs)
     accept any expr, including object and array literals.
   - op_key, match_key, mode_key, timeout_action and the cookieJar patterns
     are enforced by the validator; the parser accepts any string there.
   - Empty `.expect()` / `.check()` / `.store({})` / `expect: []` blocks are
     *validation* errors (EMPTY_SCOPE_BLOCK, EMPTY_STORE_BLOCK,
     EMPTY_ASSERT_BLOCK, §12), not parse errors -- parsers accept them. *)
```

## Lexical Rules

```ebnf
(* Variable tokens. The "." / "[n]" path suffixes are parsed at the
   grammar level -- see script_var / run_var in §2.1. *)
SCRIPT_VAR  = "$"  IDENT ;
RUN_VAR     = "$$" IDENT ;
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

Variable interpolation (`$var`, `$$var`, `${$var}`, `${$$var}`) is a semantic operation applied to string values during execution, not a lexer concern. The lexer emits the entire string as a single token. The `\$` escape yields a `$` character in the string body, but interpolation is applied to that resulting text, so `\$name` still interpolates when `name` is a variable -- there is no escape that suppresses a reference to an existing variable (spec section 3.5).

## Reserved Keywords

The ANTLR grammar declares every keyword as its own lexer token (`KW_*` in `lacelang.g4`), so these words never lex as a plain `IDENT`:

| Group | Keywords |
|---|---|
| HTTP methods | `get` `post` `put` `patch` `delete` |
| Chain methods | `expect` `check` `assert` `store` `wait` |
| Call fields | `headers` `body` `cookies` `cookieJar` `clearCookies` `redirects` `security` `timeout` |
| Config sub-fields | `follow` `max` `rejectInvalidCerts` `ms` `action` `retries` |
| Scope names | `status` `bodySize` `totalDelayMs` `dns` `connect` `tls` `ttfb` `transfer` `size` (plus `body`, `headers`, `redirects` above) |
| Scope / condition fields | `value` `op` `match` `mode` `options` `condition` |
| Helper functions | `json` `form` `schema` |
| References and literals | `this` `prev` `null` |
| Comparison operators | `eq` `neq` `lt` `lte` `gt` `gte` |
| Logical operators | `and` `or` `not` |

`true` and `false` are lexed as boolean literals.

Keywords are still accepted wherever the spec's `IDENT` names a key or a path segment: the grammar's `identKey` rule admits `IDENT` plus every keyword token and is used for object-literal keys, bare `.store()` keys, and the `this.*`, `prev.*`, `$var.*` and `$$var.*` path segments. So `this.body`, `{ status: 1 }` and `.store({ size: this.body.size })` are valid. The positions that take a plain `IDENT` -- extension-registered field names in call config, `redirects`, `security`, `timeout` and `options`, and extension function names -- cannot be a keyword. See [Grammar Notes](../implementers/notes/grammar.md).

## Operator Precedence

From lowest to highest:

| Level | Operators | Associativity |
|---|---|---|
| 1 | `or` | Left |
| 2 | `and` | Left |
| 3 | `eq`, `neq` | Non-chaining |
| 4 | `lt`, `lte`, `gt`, `gte` | Non-chaining |
| 5 | `+`, `-` | Left |
| 6 | `*`, `/`, `%` | Left |
| 7 | `not`, unary `-` | Right (prefix) |

## Notes

- **Short-circuit evaluation:** `false and f()` does not evaluate `f()`. `true or f()` does not evaluate `f()`.
- **Non-chaining comparisons:** `a eq b eq c` is a parse error. Use `(a eq b) and (b eq c)`.
- **Left-associative:** `1 - 2 - 3` = `(1 - 2) - 3`.
- **Integer overflow:** Undefined. Executors should document their integer representation. Portable scripts should stay within signed 53-bit range (2^53 - 1).
- **Trailing commas:** Accepted in all list and object positions where the grammar permits them.
