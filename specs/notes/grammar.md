# Grammar Notes

> Companion to `lacelang.g4` and `lace-spec.md`.
> Documents intentional divergences between the ANTLR4 grammar and the
> spec EBNF.

The ANTLR4 grammar `lacelang.g4` is the authoritative syntax definition. Where
it diverges from the EBNF in `lace-spec.md §2.1`, the divergence is either an
ANTLR-required adaptation that preserves spec semantics, or a permissive-parser
/ strict-validator choice consistent with spec §12.

---

## ANTLR-required adaptations (semantic-preserving)

### Explicit keyword tokens

Spec §2.2 defines `IDENT = [a-zA-Z_][a-zA-Z0-9_]*` and uses string literals
(`"get"`, `"body"`, `"expect"`, ...) directly in parser rules. The spec
lexer doesn't separate keywords from identifiers — it relies on the parser
to disambiguate by context.

ANTLR4's lexer is greedy and non-backtracking. If `'body'` appears as an
implicit token in a parser rule alongside an `IDENT` token, the lexer must
choose one tokenisation up-front, which produces ambiguity in any context
where both are valid (e.g., a JSON body literal `{ body: 1 }` inside a
request body argument).

The grammar therefore declares every keyword as a separate lexer token
(`KW_GET`, `KW_BODY`, `KW_EXPECT`, ...). To restore the spec's IDENT
semantics in positions that accept "any identifier-shaped word", the
grammar introduces an `identKey` parser rule that admits `IDENT` plus
every keyword token. This is used in object keys, store keys, and
`this.*` / `prev.*` field access — exactly the positions where the spec's
`IDENT` would naturally include keyword-shaped words.

**Semantic equivalence**: every input the spec lexer would tokenise as
`IDENT` is accepted by `identKey`. No input is rejected that the spec
would accept.

---

## Permissive parser, strict validator (spec §12 architecture)

### Generic function calls in `expr`

Spec §2.1 `helper_call` lists only `json`, `form`, `schema`. The grammar's
`funcCall` rule accepts any `IDENT '(' funcArgs? ')'` inside `expr`.

This is required because extension function calls (`template("name")`,
`text("...")`, `structured({...})`) appear in option contexts which
transitively recurse through `expr`. The spec's `expr` rule includes
`composite_lit` (objects and arrays) via `primary`, so extension field
values like `notification: { "lt": template(...) }` are fully expressible.

The validator restricts function names: in core expression contexts, only
`json`/`form`/`schema` are valid — plus the assert-only `count`/`includes`
inside an `.assert()` condition (spec §8.1). In option and
extension-registered field contexts, extension-registered function names
(including tag constructors such as `template`, `text`, `op_map`) are also
valid.

### Function-call argument types

Spec `helper_call` requires specific arg types: `json` and `form` take
`object_lit`; `schema` takes `script_var`. The grammar's `funcCall` accepts
any `expr | objectLit` as args. The validator checks arg type per known
function name.

### `cookieJar` value pattern

Spec §3.3 restricts to specific patterns (`inherit`, `fresh`,
`selective_clear`, `named:{name}`, `{name}:selective_clear`). Grammar
accepts any `STRING`. The validator enforces the pattern.

### `timeout_action` value

Spec restricts to `"fail" | "warn" | "retry"`. Grammar accepts any STRING.
The validator enforces.

### `op_key` value in `scopeObjField`

Spec restricts to `lt | lte | eq | neq | gte | gt`. Grammar accepts any
STRING. The validator enforces.

### `match` / `mode` values in `scopeObjField`

Spec restricts `match` to `first | last | any` and `mode` to
`loose | strict`. Grammar accepts any STRING. No error code covers an
invalid value yet; validators treat it as `OP_VALUE_INVALID`-style
rejection at their discretion until one is registered.

### Bare `$name` store keys

Spec §2.1 `store_key` admits `run_var | script_var | IDENT | string`; the
grammar's `storeKey` accepts the `SCRIPT_VAR` token for the `$name`
write-back form (§4.6).

### Extension-field values

The EBNF writes extension fallthroughs as `IDENT ":" expr`. The grammar's
`optionsValue` additionally admits object and array literals directly, so
`notification: { "404": template("x") }` parses without going through
`expr`.

### Extension-field key restriction

`callField`, `redirectsField`, `securityField`, `timeoutField`, and
`optionsField` all have an extension fallthrough. The grammar uses `IDENT`
(lexer-level non-keyword token), not `identKey`, for these positions.
This forces extension-registered field names to be non-keyword identifiers.

**Rationale**: using `identKey` here would silently accept wrong-typed
values for built-in fields (e.g. `headers: "string"` matching the
extension fallthrough after the `KW_HEADERS ':' objectLit` branch failed).
Restricting to `IDENT` makes extension field names disjoint from built-in
keywords and gives correct parse errors for wrong-typed built-in fields.

### Empty blocks in `.expect()`, `.check()`, `.store()`

The parser accepts empty blocks (e.g. `.store({})`, `.expect()` with no
scopes, `expect: []`). The validator rejects these with
`EMPTY_SCOPE_BLOCK`, `EMPTY_STORE_BLOCK` or `EMPTY_ASSERT_BLOCK`.

This is consistent with the permissive-parser / strict-validator
architecture: the parser produces an AST node, and the validator enforces
the semantic constraint. The grammar rules `scopeList`, `assertClause`
and `storeMethod` make their entry lists optional for this reason; the
"at least one" requirement is a validator concern (spec §12), and the
conformance vectors `02_validation/empty_*_block.json` require the empty
forms to parse.
