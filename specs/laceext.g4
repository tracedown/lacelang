/*
 * Lace extension rule/function DSL — ANTLR4 grammar
 *
 * Canonical syntax definition for text that appears inside the triple-
 * quoted `body` fields of `[[rules.rule]]` and `[functions.X]` in a
 * `.laceext` TOML file. Conforms to lace-extensions.md §5.1.
 *
 * Scope:
 *   - This grammar covers ONLY the rule/function body language.
 *   - The surrounding `.laceext` document is TOML — parse it with any
 *     TOML 1.0-compliant parser; pull the `body` strings out, then feed
 *     each one to a parser generated from this grammar.
 *
 * Indentation:
 *   The DSL is Python-style indentation-sensitive: `for`, `when`,
 *   function bodies, and rule bodies introduce INDENT/DEDENT scopes.
 *   ANTLR4 has no built-in support for indentation. Every consuming
 *   language needs a pre-lexer wrapper that tracks the indent stack and
 *   emits synthetic INDENT / DEDENT / NEWLINE tokens — the same pattern
 *   used by ANTLR's reference Python grammar. The two synthetic token
 *   names below are declared in a `tokens {}` block so the parser rules
 *   can reference them; the lexer itself produces no INDENT / DEDENT
 *   tokens on its own.
 *
 *   The reference pre-lexer is the Python module
 *   `lacelang_executor/laceext/dsl_lexer.py`; ports to other runtimes
 *   should mirror its behaviour:
 *     - `\n` inside open (…) / […] / {…} is whitespace, not NEWLINE.
 *     - At top-of-line, compare indent width to stack top:
 *         deeper  → push, emit INDENT
 *         equal   → emit NEWLINE and continue at same level
 *         shallower → emit matching count of DEDENTs
 *     - Blank lines and `#`-comment-only lines are skipped.
 *
 * Qualified function calls:
 *   `extName.fnName(args)` invokes an exposed function owned by a peer
 *   extension (see lace-extensions.md §6.1). Extension names are
 *   camelCase IDENTs (§2), so the qualified form is unambiguous and
 *   parses as `IDENT DOT IDENT LPAREN …`.
 *
 * Comparison operators:
 *   Comparisons use keyword forms `eq` / `neq` / `lt` / `lte` / `gt` /
 *   `gte` (same vocabulary as the scope `op:` field).
 */

grammar laceext;

tokens {
    INDENT,  // produced by the pre-lexer on indent increase
    DEDENT   // produced by the pre-lexer on indent decrease
}

// ====================================================================
// Parser rules
// ====================================================================

ruleBody
    : (NEWLINE | statement)* EOF
    ;

functionBody
    : (NEWLINE | statement)* returnStmt (NEWLINE)* EOF
    ;

statement
    : forStmt
    | whenStmt
    | letStmt
    | setStmt
    | emitStmt
    | exitStmt
    | callStmt
    ;

// ── Control ─────────────────────────────────────────────────────────

forStmt
    : KW_FOR BINDING KW_IN expr ':' NEWLINE INDENT block DEDENT
    ;

whenStmt
    : KW_WHEN expr ':' NEWLINE INDENT block DEDENT   # whenBlock
    | KW_WHEN expr NEWLINE                            # whenInline
    ;

block
    : (NEWLINE | statement)+
    ;

letStmt
    : KW_LET BINDING '=' expr NEWLINE
    ;

// set is only valid inside function bodies — rule bindings are immutable.
// The parser / loader enforces this restriction (same pattern as return).
setStmt
    : KW_SET BINDING '=' expr NEWLINE
    ;

emitStmt
    : KW_EMIT emitTarget '<-' objectLit NEWLINE
    ;

emitTarget
    : KW_RESULT ('.' IDENT)+
    ;

exitStmt
    : KW_EXIT NEWLINE
    ;

returnStmt
    : KW_RETURN expr NEWLINE
    ;

callStmt
    : funcCall NEWLINE
    ;

// ── Expressions ─────────────────────────────────────────────────────
//
// Precedence climb (low → high):
//   ternary < or < and < eq/neq < lt/lte/gt/gte < add/sub < mul/div
//   < unary < access < primary

expr
    : orExpr ('?' expr ':' expr)?
    ;

orExpr
    : andExpr (KW_OR andExpr)*
    ;

andExpr
    : eqExpr (KW_AND eqExpr)*
    ;

// Non-chaining: at most one equality / ordering operator per layer.
// Spec §5.3 — `a eq b eq c` is a parse error; compose with `and`/`or`
// and parentheses.
eqExpr
    : ordExpr ((KW_EQ | KW_NEQ) ordExpr)?
    ;

ordExpr
    : addSubExpr ((KW_LT | KW_LTE | KW_GT | KW_GTE) addSubExpr)?
    ;

addSubExpr
    : mulDivExpr (('+' | '-') mulDivExpr)*
    ;

mulDivExpr
    : unaryExpr (('*' | '/') unaryExpr)*
    ;

unaryExpr
    : KW_NOT unaryExpr
    | '-'    unaryExpr
    | accessExpr
    ;

// Field / index / filter access. Chains are left-associative.
accessExpr
    : primary accessOp*
    ;

accessOp
    : '.'  identKey             # fieldAccess
    | '?.' identKey             # nullSafeFieldAccess
    | '['  expr ']'             # indexAccess
    | '[?' expr ']'             # filterAccess
    ;

primary
    : '(' expr ')'                                        # parenExpr
    | objectLit                                           # objectLitExpr
    | literal                                             # literalExpr
    | base                                                # baseExpr
    | BINDING                                             # bindingExpr
    | funcCall                                            # funcCallExpr
    | IDENT                                               # identExpr
    ;

// Extensions declare functions locally (`fnName(...)`) or call an
// exposed function exported by a `require`d peer extension
// (`extName.fnName(...)`). Extension names are camelCase IDENTs, so the
// qualified form is unambiguous — a single IDENT on each side of the
// dot, no lexical ambiguity with subtraction / field access.
funcCall
    : IDENT '.' IDENT '(' argList? ')'    # qualifiedCall
    | IDENT           '(' argList? ')'    # localCall
    ;

argList
    : expr (',' expr)* ','?
    ;

// Bases are the reserved roots of the access chain. `result` is only
// valid on the left of `<-` in emit targets (enforced in rule bodies by
// static rules above); at expression position it yields the frozen view
// of the current result, which is rarely useful but always defined.
base
    : KW_RESULT
    | KW_PREV
    | KW_THIS
    | KW_CONFIG
    | KW_REQUIRE
    ;

// `identKey` is the union of IDENT and every keyword — mirrors the
// lacelang.g4 rule of the same name. Lets field names collide with DSL
// keywords (e.g. `call.config.timeout?.notification`).
identKey
    : IDENT
    | KW_FOR | KW_IN | KW_WHEN | KW_LET | KW_SET | KW_EMIT | KW_EXIT | KW_RETURN
    | KW_AND | KW_OR  | KW_NOT
    | KW_TRUE | KW_FALSE | KW_NULL
    | KW_RESULT | KW_PREV | KW_THIS | KW_CONFIG | KW_REQUIRE
    | KW_EQ | KW_NEQ | KW_LT | KW_LTE | KW_GT | KW_GTE
    ;

// ── Object literals ─────────────────────────────────────────────────

objectLit
    : '{' '}'
    | '{' objectEntry (',' objectEntry)* ','? '}'
    ;

objectEntry
    : (STRING | IDENT) ':' expr
    ;

// ── Literals ────────────────────────────────────────────────────────

literal
    : STRING
    | INT
    | FLOAT
    | KW_TRUE
    | KW_FALSE
    | KW_NULL
    ;

// ====================================================================
// Lexer rules
// ====================================================================
//
// Keyword-first ordering matters: ANTLR resolves by declaration order
// when two rules match the same prefix. Keywords appear before IDENT so
// `eq`/`neq`/etc. are lexed as comparison operators, not identifiers.

// Control keywords
KW_FOR     : 'for' ;
KW_IN      : 'in' ;
KW_WHEN    : 'when' ;
KW_LET     : 'let' ;
KW_SET     : 'set' ;
KW_EMIT    : 'emit' ;
KW_EXIT    : 'exit' ;
KW_RETURN  : 'return' ;

// Logical operator keywords
KW_AND     : 'and' ;
KW_OR      : 'or' ;
KW_NOT     : 'not' ;

// Literal keywords
KW_TRUE    : 'true' ;
KW_FALSE   : 'false' ;
KW_NULL    : 'null' ;

// Root / base keywords
KW_RESULT  : 'result' ;
KW_PREV    : 'prev' ;
KW_THIS    : 'this' ;
KW_CONFIG  : 'config' ;
KW_REQUIRE : 'require' ;

// Comparison operator keywords
KW_EQ      : 'eq' ;
KW_NEQ     : 'neq' ;
KW_LT      : 'lt' ;
KW_LTE     : 'lte' ;
KW_GT      : 'gt' ;
KW_GTE     : 'gte' ;

// Identifier — lowercase/underscore leading, camelCase follow. (Extension
// *names* from §2 are more restrictive — [a-z][A-Za-z0-9]* — but the DSL
// itself accepts standard identifier characters so local function names
// and field accessors can use underscores.)
IDENT
    : [a-zA-Z_] [a-zA-Z0-9_]*
    ;

// Bindings are scope-locals written with a leading `$`. Bare `$` (no
// identifier body) is the array-filter current-element reference used
// inside `[? predicate]` expressions.
BINDING
    : '$' [a-zA-Z_] [a-zA-Z0-9_]*
    | '$'
    ;

// Literals
INT        : [0-9]+ ;
FLOAT      : [0-9]+ '.' [0-9]+ ;

STRING
    : '"'  (ESC_SEQ | ~["\\])* '"'
    | '\'' (ESC_SEQ | ~['\\])* '\''
    ;

fragment ESC_SEQ
    : '\\' [\\"'nrt]
    ;

// Multi-char punctuation (longer matches first)
ARROW      : '<-' ;
QDOT       : '?.' ;
QBRACK     : '[?' ;

// Single-char punctuation
LPAREN     : '(' ;
RPAREN     : ')' ;
LBRACK     : '[' ;
RBRACK     : ']' ;
LBRACE     : '{' ;
RBRACE     : '}' ;
COMMA      : ',' ;
COLON      : ':' ;
DOT        : '.' ;
PLUS       : '+' ;
MINUS      : '-' ;
STAR       : '*' ;
SLASH      : '/' ;
QUESTION   : '?' ;
EQUALS     : '=' ;

// Newlines inside open brackets are whitespace (handled by pre-lexer).
// At top-level, pre-lexer emits NEWLINE / INDENT / DEDENT.
NEWLINE    : '\r'? '\n' ;

// `#` line comments run to end of line.
LINE_COMMENT : '#' ~[\r\n]* -> skip ;

// Spaces / tabs inside a line are skipped. Indent detection happens in
// the pre-lexer, which inspects the raw text before this lexer runs.
WS           : [ \t]+ -> skip ;
