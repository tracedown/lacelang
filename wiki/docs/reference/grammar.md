# Grammar Reference

Lace v0.9.1 grammar. The ANTLR4 grammar (`lacelang.g4`) is the authoritative syntax definition. The EBNF below is the human-readable mirror from the spec.

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
                | "match"   ":" match_key      (* redirects scope only *)
                | "mode"    ":" mode_key       (* body: schema(...) only *)
                | "options" ":" options_obj ;

match_key       = '"first"' | '"last"' | '"any"' ;
mode_key        = '"loose"' | '"strict"' ;

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

(* Expression grammar — precedence from lowest to highest:
 *   or -> and -> eq/neq -> lt/lte/gt/gte -> +/- -> * / / % -> unary not/- -> primary
 * Comparisons do not chain: `a eq b eq c` is a parse error.
 * `and` and `or` use short-circuit evaluation.
 * Binary operators are left-associative. *)

expr        = or_expr ;

or_expr     = and_expr ("or" and_expr)* ;
and_expr    = cmp_expr ("and" cmp_expr)* ;

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

size_string     = STRING matching /\d+(k|kb|m|mb|g|gb)?/i ;
```

## Lexical Rules

```ebnf
script_var  = "$"  IDENT ;
run_var     = "$$" IDENT ;
IDENT       = [a-zA-Z_][a-zA-Z0-9_]* ;

string      = '"' string_char* '"' ;
string_char = any_char_except_dquote_and_backslash | escape_seq ;
escape_seq  = "\" ('"' | "\" | "n" | "t" | "r" | "$") ;

integer_lit = [0-9]+ ;
float_lit   = [0-9]+ "." [0-9]+ ;
bool_lit    = "true" | "false" ;
comment     = "//" [^\n]* (newline | EOF) ;
(* Whitespace ignored between tokens *)
```

Variable interpolation (`$var`, `$$var`, `${$var}`, `${$$var}`) is a semantic operation applied to string values during execution, not a lexer concern. The lexer emits the entire string as a single token.

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
- **Integer overflow:** Undefined. Portable scripts should stay within signed 53-bit range.
- **Trailing commas:** Accepted in all list and object positions where the grammar permits them.
