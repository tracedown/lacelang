# Helper Functions

Core helper functions available in Lace expressions. These are the only functions permitted in core expression contexts (outside extension-registered option fields).

Spec version: 0.9.0

## `json(object) -> string`

Serialises an object literal to a JSON string. Variable interpolation is applied to string values before serialisation.

Primarily used to set the request body:

```lace
post("$BASE_URL/login", {
  body: json({ email: "$admin_email", password: "$admin_pass" })
})
```

The executor automatically sets `Content-Type: application/json` when `json()` is used as the body value.

**Argument type:** Must be an object literal (`{...}`). Passing a string or other type is a validation error (`FUNC_ARG_TYPE`).

## `form(object) -> string`

Serialises an object literal to a URL-encoded form string (`application/x-www-form-urlencoded`). Variable interpolation is applied to string values before serialisation.

```lace
post("$BASE_URL/login", {
  body: form({ username: "$user", password: "$pass" })
})
```

The executor automatically sets `Content-Type: application/x-www-form-urlencoded` when `form()` is used as the body value.

**Argument type:** Must be an object literal (`{...}`). Passing a string or other type is a validation error (`FUNC_ARG_TYPE`).

## `schema($var) -> schema_ref`

Creates a JSON Schema validation reference for body matching. The variable must contain a valid JSON Schema document string.

```lace
get("$BASE_URL/api/users")
.expect(
  status: 200,
  body: schema($user_schema)
)
```

With match mode:

```lace
.expect(
  body: { value: schema($user_schema), mode: "strict" }
)
```

**Argument type:** Must be a script variable reference (`$var`). Passing a literal or other expression is a validation error (`FUNC_ARG_TYPE`).

**Null handling:**

- `schema($var)` where `$var` resolves to `null` is a hard fail at runtime.
- `schema($var)` where `$var` is not in the variable registry is a validation error (`SCHEMA_VAR_UNKNOWN`).
- When the response body is not valid JSON, the schema check hard-fails.
