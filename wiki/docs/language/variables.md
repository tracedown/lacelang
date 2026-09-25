# Variables

Lace has two kinds of variables: **script variables** (`$var`) injected before the run, and **run-scope variables** (`$$var`) created during the run by `.store()`.

## Script Variables (`$var`)

Script variables are provided by the backend as a flat key-value map before execution starts. They are **read-only** during the run --- their in-memory value never changes.

```lace
get("$BASE_URL/api/users", {
  headers: { Authorization: "Bearer $api_key" }
})
.expect(status: 200)
```

From the CLI, inject variables with `--vars` (a JSON object file) or `--var` (individual values):

```bash
lacelang-executor run script.lace --vars vars.json
lacelang-executor run script.lace --var BASE_URL=https://api.example.com --var api_key=abc123
```

Multiple `--var` flags merge, and a `--var` overrides the same key from `--vars`.

### Unknown variables

When the validator is given a variable registry (the list of declared names, e.g. `--vars-list` on `validate`), a `$var` reference that is not in it is a validation error (`VARIABLE_UNKNOWN`), and so is a `schema($var)` argument. Without a registry, a missing `$var` simply resolves to `null` at runtime (see [Null Semantics](#null-semantics)).

**Schema variables** are script variables whose value is a valid JSON Schema document. They are used only with `schema($var)` in body matching.

## Run-Scope Variables (`$$var`)

Run-scope variables are created by `.store()` with the `$$` prefix. Once created, they are available to all subsequent chain methods and calls.

```lace
post("$BASE_URL/login", {
  body: json({ email: "$email", password: "$pass" })
})
.expect(status: 200)
.store({ "$$token": this.body.access_token })

// $$token is now available
get("$BASE_URL/api/profile", {
  headers: { Authorization: "Bearer $$token" }
})
.expect(status: 200)
```

Run-scope variables appear in the result's `runVars` object for debugging and backend use.

### Write-Once Rule

A `$$var` can be assigned **exactly once** across the entire script. A second assignment to the same key is a **validation error** --- the validator rejects the script before it runs.

```lace
// INVALID --- $$token assigned twice
get("$BASE_URL/token")
.expect(status: 200)
.store({ "$$token": this.body.token })

get("$BASE_URL/token/refresh")
.expect(status: 200)
.store({ "$$token": this.body.new_token })   // validation error
```

## .store() Key Types

`.store()` accepts three key formats that determine where the value goes. Keys may be quoted strings or written bare:

```lace
get("$BASE_URL/api/items")
.expect(status: 200)
.store({
  "$$token":    this.body.access_token,   // run-scope variable
  "$cursor":    this.body.next_cursor,    // write-back to backend
  last_count:   this.body.count,          // write-back to backend (same as $cursor)
  $$page:       this.body.page            // bare form of "$$page"
})
```

| Key format | Where it goes | Behaviour |
|---|---|---|
| `$$name` | `runVars` in the result | Write-once. Available to subsequent calls in this run. Never appears in `actions.variables`. |
| `$name` | `actions.variables` in the result | The `$` is stripped from the key. The variable's in-memory value does **not** change during this run. The backend decides what to do with it. |
| `plain_name` | `actions.variables` in the result | Equivalent to `$name`. The backend decides scope and persistence. |

`actions.variables` is present in the result only when the script stores at least one write-back key.

Values can be any JSON-serialisable type: string, integer, float, boolean, null, object, or array.

!!! note
    `.store()` is skipped if a preceding chain method on the same call produced a hard fail (from `.expect()` or `.assert({ expect: [...] })`).

## Path Access

In expressions, a variable holding an object or array can be navigated with `.field` and `[n]` suffixes, the same way as `prev` (`this` takes `.field` suffixes only):

```lace
get("$BASE_URL/api/users")
.expect(status: 200)
.store({ "$$users": this.body.users })

get("$BASE_URL/api/profile")
.expect(status: 200)
.assert({
  expect: [
    this.body.email eq $$users[0].email,
    this.body.plan eq $$users[0].plan
  ]
})
```

The same suffixes are accepted on script variables (`$config.region`). Paths work in expressions only. String interpolation takes a bare variable name --- `"$$users.count"` interpolates `$$users` followed by the literal text `.count`.

## String Interpolation

Variables are interpolated inside quoted strings throughout the script --- in URLs, header values, body content, and anywhere else a string appears.

| Syntax | Resolves to |
|---|---|
| `$varname` | Script variable value |
| `$$varname` | Run-scope variable value |
| `${$varname}` | Disambiguated script variable |
| `${$$varname}` | Disambiguated run-scope variable |
Use the braced form when the variable name runs into surrounding text:

```lace
get("$BASE_URL/api/${$resource_type}s/${$$item_id}")
.expect(status: 200)
```

There is no way to escape a reference. The string escape `\$` produces a `$` character, but interpolation is applied to the resulting text, so `"\$name"` still interpolates when `name` is a variable. Braced forms only control where a reference ends.

## Null Semantics

Lace has specific rules for how null values behave:

| Situation | Result |
|---|---|
| `$var` not in the injected map | `null` |
| `$$var` not yet assigned | `null` |
| `prev.*` with no previous results | `null` |
| `null` in string interpolation | Literal string `"null"` + warning recorded |
| `null eq null` | `true` |
| `null eq value` | `false` |
| `null neq value` | `true` |
| `null` in `lt`, `gt`, `lte`, `gte` | **indeterminate** (not an error, execution continues) |
| `null` in `+`, `-`, `*`, `/`, `%` | **indeterminate** |
| `schema($var)` where `$var` is `null` | **hard fail** |

!!! warning "Null interpolation warning"
    When a null variable is interpolated into a string, the literal text `"null"` is inserted and a warning is recorded. This is almost never what you want --- check that your variables are injected correctly.
