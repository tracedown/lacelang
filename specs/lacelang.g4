/*
 * Lace probe scripting language — ANTLR4 grammar
 *
 * Canonical syntax definition for `.lace` source text. Conforms to
 * lace-spec.md.
 *
 * Chain method order: .expect → .check → .assert → .store → .wait
 * Each chain method is optional; each appears at most once per call.
 *
 * The `options {}` block on scopes and conditions is a syntactic
 * placeholder for extensions. The core executor passes it through
 * opaquely. Unknown identifiers and function calls inside expressions
 * are accepted by the parser; the validator (per spec §12) rejects
 * unknown function calls outside extension contexts.
 *
 * Variable interpolation inside string literals is recognised at the
 * lexer level. The parser treats the string body as a single token;
 * consumers re-scan the body for $var / $$var / ${...} interpolation.
 */

grammar lacelang;

// ====================================================================
// Parser rules
// ====================================================================

script
    : call+ EOF
    ;

call
    : httpMethod '(' urlArg (',' callConfig)? ')' chainMethod+
    ;

httpMethod
    : KW_GET | KW_POST | KW_PUT | KW_PATCH | KW_DELETE
    ;

urlArg
    : STRING
    ;

// ── Call config ─────────────────────────────────────────────────────

callConfig
    : '{' callField (',' callField)* ','? '}'
    ;

// Per-field rules use explicit keyword tokens for type-correct values.
// Unknown identifiers are accepted (extensions can register call fields).
callField
    : KW_HEADERS      ':' objectLit
    | KW_BODY         ':' bodyValue
    | KW_COOKIES      ':' objectLit
    | KW_COOKIE_JAR   ':' STRING
    | KW_CLEAR_COOKIES ':' '[' STRING (',' STRING)* ','? ']'
    | KW_REDIRECTS    ':' redirectsObj
    | KW_SECURITY     ':' securityObj
    | KW_TIMEOUT      ':' timeoutObj
    | IDENT           ':' optionsValue           // extension-registered (non-keyword)
    ;

bodyValue
    : KW_JSON '(' objectLit ')'
    | KW_FORM '(' objectLit ')'
    | STRING
    ;

redirectsObj
    : '{' redirectsField (',' redirectsField)* ','? '}'
    ;

redirectsField
    : KW_FOLLOW ':' BOOL
    | KW_MAX    ':' INT
    | IDENT     ':' optionsValue                 // extension field (non-keyword)
    ;

securityObj
    : '{' securityField (',' securityField)* ','? '}'
    ;

securityField
    : KW_REJECT_INVALID_CERTS ':' BOOL
    | IDENT                   ':' optionsValue   // extension field (non-keyword)
    ;

timeoutObj
    : '{' timeoutField (',' timeoutField)* ','? '}'
    ;

timeoutField
    : KW_MS      ':' INT
    | KW_ACTION  ':' STRING                      // "fail" | "warn" | "retry"
    | KW_RETRIES ':' INT
    | IDENT      ':' optionsValue                // extension field (non-keyword)
    ;

// ── Chain methods ───────────────────────────────────────────────────
// Fixed order: .expect → .check → .assert → .store → .wait
// Each appears at most once. Validator enforces order and uniqueness.

chainMethod
    : expectMethod
    | checkMethod
    | assertMethod
    | storeMethod
    | waitMethod
    ;

expectMethod : '.' KW_EXPECT '(' scopeList ')' ;
checkMethod  : '.' KW_CHECK  '(' scopeList ')' ;

scopeList
    : scopeEntry (',' scopeEntry)* ','?
    ;

scopeEntry
    : scopeName ':' scopeVal
    ;

scopeName
    : KW_STATUS     | KW_BODY      | KW_HEADERS   | KW_BODY_SIZE
    | KW_TOTAL_DELAY_MS | KW_DNS   | KW_CONNECT   | KW_TLS
    | KW_TTFB       | KW_TRANSFER  | KW_SIZE      | KW_REDIRECTS
    ;

scopeVal
    : expr                                                  // shorthand: any expression
    | '[' expr (',' expr)* ','? ']'                         // array shorthand
    | '{' scopeObjField (',' scopeObjField)* ','? '}'       // value+op or full form
    ;

scopeObjField
    : KW_VALUE   ':' scopeValueExpr
    | KW_OP      ':' STRING                                 // "lt" | "lte" | ...
    | KW_MATCH   ':' STRING                                 // "first" | "last" | "any" (redirects scope)
    | KW_MODE    ':' STRING                                 // "strict" | "loose" (body schema)
    | KW_OPTIONS ':' optionsObj
    ;

scopeValueExpr
    : expr
    | '[' expr (',' expr)* ','? ']'
    ;

// `options {}` is a syntactic placeholder. All field content is
// extension-defined; the core executor and parser do not interpret it
// beyond the structural shape.
optionsObj
    : '{' '}'
    | '{' optionsField (',' optionsField)* ','? '}'
    ;

// Spec §2.1 options_field = IDENT ":" expr — IDENT only (no STRING, no
// keywords). Inner objectLits inside the value (e.g. notification op_map)
// still allow STRING keys via objectEntry.
optionsField
    : IDENT ':' optionsValue
    ;

// Permissive value grammar: any expression, object literal, array,
// or function call (extension-registered functions like template/text).
optionsValue
    : expr
    | objectLit
    | '[' (optionsValue (',' optionsValue)* ','?)? ']'
    ;

assertMethod
    : '.' KW_ASSERT '(' '{' assertClause (',' assertClause)* ','? '}' ')'
    ;

assertClause
    : (KW_EXPECT | KW_CHECK) ':' '[' conditionItem (',' conditionItem)* ','? ']'
    ;

conditionItem
    : expr
    | '{' condField (',' condField)* ','? '}'
    ;

condField
    : KW_CONDITION ':' expr
    | KW_OPTIONS   ':' optionsObj
    ;

storeMethod
    : '.' KW_STORE '(' '{' storeEntry (',' storeEntry)* ','? '}' ')'
    ;

storeEntry
    : storeKey ':' expr
    ;

storeKey
    : RUN_VAR        // $$name → run-scope, write-once
    | SCRIPT_VAR     // $name → write-back ($ stripped in result)
    | identKey       // bare identifier → write-back
    | STRING         // quoted key — write-back unless source is "$$..."
    ;

waitMethod
    : '.' KW_WAIT '(' INT ')'
    ;

// ── Object literals ────────────────────────────────────────────────

objectLit
    : '{' '}'
    | '{' objectEntry (',' objectEntry)* ','? '}'
    ;

objectEntry
    : (STRING | identKey) ':' expr
    ;

// `identKey` is the union of IDENT plus every keyword. Lets keywords
// be used as object/option keys without lexer ambiguity.
identKey
    : IDENT
    | KW_GET | KW_POST | KW_PUT | KW_PATCH | KW_DELETE
    | KW_HEADERS | KW_BODY | KW_COOKIES | KW_COOKIE_JAR | KW_CLEAR_COOKIES
    | KW_REDIRECTS | KW_SECURITY | KW_TIMEOUT
    | KW_FOLLOW | KW_MAX
    | KW_REJECT_INVALID_CERTS
    | KW_MS | KW_ACTION | KW_RETRIES
    | KW_EXPECT | KW_CHECK | KW_ASSERT | KW_STORE | KW_WAIT
    | KW_STATUS | KW_BODY_SIZE | KW_TOTAL_DELAY_MS | KW_DNS | KW_CONNECT
    | KW_TLS | KW_TTFB | KW_TRANSFER | KW_SIZE
    | KW_VALUE | KW_OP | KW_MATCH | KW_MODE | KW_OPTIONS | KW_CONDITION
    | KW_JSON | KW_FORM | KW_SCHEMA
    | KW_THIS | KW_PREV | KW_NULL
    | KW_OP_EQ | KW_OP_NEQ | KW_OP_LT | KW_OP_LTE | KW_OP_GT | KW_OP_GTE
    | KW_AND | KW_OR | KW_NOT
    ;

// ── Expressions ────────────────────────────────────────────────────
//
// Layered precedence (low → high):
//   or < and < eq/neq < lt/lte/gt/gte < +/- < */ /% < unary < primary
//
// Comparisons do NOT chain — `a eq b eq c` is a parse error; compose
// with `(a eq b) and (b eq c)` instead. Parentheses are the only
// precedence override.

expr
    : orExpr
    ;

orExpr
    : andExpr (KW_OR andExpr)*
    ;

andExpr
    : eqExpr (KW_AND eqExpr)*
    ;

// Non-chaining: at most one equality operator per sub-expression.
eqExpr
    : ordExpr (op=(KW_OP_EQ|KW_OP_NEQ) ordExpr)?
    ;

ordExpr
    : addSubExpr (op=(KW_OP_LT|KW_OP_LTE|KW_OP_GT|KW_OP_GTE) addSubExpr)?
    ;

addSubExpr
    : mulDivExpr (op=('+'|'-') mulDivExpr)*
    ;

mulDivExpr
    : unaryExpr (op=('*'|'/'|'%') unaryExpr)*
    ;

unaryExpr
    : KW_NOT unaryExpr                                              # NotExpr
    | '-'    unaryExpr                                              # NegExpr
    | primary                                                       # PrimaryExpr
    ;

primary
    : '(' expr ')'                                                  # ParenExpr
    | thisRef                                                       # ThisExpr
    | prevRef                                                       # PrevExpr
    | funcCall                                                      # FuncCallExpr
    | runVarRef                                                     # RunVarExpr
    | scriptVarRef                                                  # ScriptVarExpr
    | valueLiteral                                                  # LitExpr
    | objectLit                                                     # ObjectLitExpr2
    | '[' (expr (',' expr)* ','?)? ']'                              # ArrayLitExpr
    ;

thisRef
    : KW_THIS ('.' identKey)+
    ;

prevRef
    : KW_PREV (('.' identKey) | ('[' INT ']'))*
    ;

// Script / run-scope variable references admit the same dot / index path
// as `prev`, so callers can destructure injected JSON without a wrapping
// `this.*` or a prior `.store()`.
scriptVarRef
    : SCRIPT_VAR (('.' identKey) | ('[' INT ']'))*
    ;

runVarRef
    : RUN_VAR (('.' identKey) | ('[' INT ']'))*
    ;

// Generic function call. The validator (spec §12) rejects calls to
// unknown function names outside extension-permitted contexts. Allowed
// in core: json, form, schema. Extensions register additional names
// (e.g. template, text from laceNotifications).
funcCall
    : (KW_JSON | KW_FORM | KW_SCHEMA | IDENT) '(' funcArgs? ')'
    ;

funcArgs
    : funcArg (',' funcArg)* ','?
    ;

funcArg
    : expr
    | objectLit
    ;

valueLiteral
    : STRING
    | INT
    | FLOAT
    | BOOL
    | KW_NULL
    ;

// ====================================================================
// Lexer rules
// ====================================================================

// Reserved-name lexing: RUN_VAR > SCRIPT_VAR > keywords > IDENT.
// ANTLR resolves by declaration order and longest match.

RUN_VAR     : '$$' [a-zA-Z_] [a-zA-Z0-9_]* ;
SCRIPT_VAR  : '$' [a-zA-Z_] [a-zA-Z0-9_]* ;

// HTTP methods
KW_GET    : 'get' ;
KW_POST   : 'post' ;
KW_PUT    : 'put' ;
KW_PATCH  : 'patch' ;
KW_DELETE : 'delete' ;

// Chain methods
KW_EXPECT : 'expect' ;
KW_CHECK  : 'check' ;
KW_ASSERT : 'assert' ;
KW_STORE  : 'store' ;
KW_WAIT   : 'wait' ;

// Call config keys
KW_HEADERS        : 'headers' ;
KW_BODY           : 'body' ;
KW_COOKIES        : 'cookies' ;
KW_COOKIE_JAR     : 'cookieJar' ;
KW_CLEAR_COOKIES  : 'clearCookies' ;
KW_REDIRECTS      : 'redirects' ;
KW_SECURITY       : 'security' ;
KW_TIMEOUT        : 'timeout' ;

// Sub-object keys
KW_FOLLOW              : 'follow' ;
KW_MAX                 : 'max' ;
KW_REJECT_INVALID_CERTS : 'rejectInvalidCerts' ;
KW_MS                  : 'ms' ;
KW_ACTION              : 'action' ;
KW_RETRIES             : 'retries' ;

// Scope names
KW_STATUS       : 'status' ;
KW_BODY_SIZE     : 'bodySize' ;
KW_TOTAL_DELAY_MS : 'totalDelayMs' ;
KW_DNS          : 'dns' ;
KW_CONNECT      : 'connect' ;
KW_TLS          : 'tls' ;
KW_TTFB         : 'ttfb' ;
KW_TRANSFER     : 'transfer' ;
KW_SIZE         : 'size' ;

// Scope object keys
KW_VALUE     : 'value' ;
KW_OP        : 'op' ;
KW_MATCH     : 'match' ;
KW_MODE      : 'mode' ;
KW_OPTIONS   : 'options' ;
KW_CONDITION : 'condition' ;

// Helper functions
KW_JSON   : 'json' ;
KW_FORM   : 'form' ;
KW_SCHEMA : 'schema' ;

// Reserved expression names
KW_THIS : 'this' ;
KW_PREV : 'prev' ;
KW_NULL : 'null' ;

// Comparison operator keywords
KW_OP_EQ  : 'eq' ;
KW_OP_NEQ : 'neq' ;
KW_OP_LT  : 'lt' ;
KW_OP_LTE : 'lte' ;
KW_OP_GT  : 'gt' ;
KW_OP_GTE : 'gte' ;

// Logical connective keywords
KW_AND    : 'and' ;
KW_OR     : 'or' ;
KW_NOT    : 'not' ;

BOOL    : 'true' | 'false' ;

IDENT   : [a-zA-Z_] [a-zA-Z0-9_]* ;

INT     : [0-9]+ ;
FLOAT   : [0-9]+ '.' [0-9]+ ;

// String literal: double-quoted, with escape sequences.
// Variable interpolation ($var, $$var, ${expr}) is part of the string
// content — consumers re-scan the body to extract interpolation refs.
STRING
    : '"' (ESC_SEQ | ~["\\])* '"'
    ;

fragment ESC_SEQ
    : '\\' [\\"nrt$]
    ;

LINE_COMMENT : '//' ~[\r\n]* -> skip ;
WS           : [ \t\r\n]+ -> skip ;
