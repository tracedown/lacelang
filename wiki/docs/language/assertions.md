# Assertions: .expect() and .check()

`.expect()` and `.check()` validate response properties using **scopes** --- named checks against parts of the response like status code, timing, body, and headers.

The only difference between them: `.expect()` failures are **hard** (execution stops), `.check()` failures are **soft** (recorded, execution continues).

## Complete Evaluation

Both methods evaluate **all** their scopes before any failure takes effect. If `.expect()` has three scopes and two fail, both failures are recorded together --- you see every problem at once, not just the first.

```lace
// If both status and timing fail, you see both failures in the result
.expect(
  status: 200,
  totalDelayMs: 500
)
```

## Scope Forms

Every scope accepts three forms.

### Shorthand --- value only

The operator is inferred from the scope name (see [Default Operators](#default-operators)):

```lace
.expect(
  status: 200,           // eq (exact match)
  totalDelayMs: 500      // lt (less than)
)
```

For `status`, you can pass an array --- any match passes:

```lace
.expect(status: [200, 201, 204])
```

### Value + operator

Override the default operator:

```lace
.expect(
  totalDelayMs: { value: 500, op: "lte" }   // less than or equal
)
```

### Full form --- with options

Includes the `options` block, which is a placeholder for extensions:

```lace
.expect(
  status: {
    value: 200,
    op: "eq",
    options: {
      // Extension fields go here (e.g. notification templates)
    }
  }
)
```

All three forms produce the same core behaviour. The full form exists so extensions can attach metadata to individual scopes.

## Available Scopes

| Scope | Type | Description |
|---|---|---|
| `status` | integer or integer[] | HTTP status code. Array means "any of these". |
| `body` | body match | Body content or schema validation. See [Body Matching](#body-matching). |
| `headers` | object | Each key-value pair must match. Keys are case-insensitive. |
| `bodySize` | size string | Body size threshold (e.g. `"50kb"`, `"10m"`). Also gates body capture. |
| `totalDelayMs` | integer | Total response time threshold in ms. |
| `dns` | integer | DNS resolution time threshold in ms. |
| `connect` | integer | TCP connection time threshold in ms. |
| `tls` | integer | TLS handshake time threshold in ms. Skipped when TLS time is 0. |
| `ttfb` | integer | Time to first byte threshold in ms. |
| `transfer` | integer | Body transfer time threshold in ms. |
| `size` | integer | Exact response body size in bytes. |
| `redirects` | string | URL match against redirect chain. See [Redirects Scope](#redirects-scope). |

**Size string format** for `bodySize`: a number optionally followed by a unit --- `50`, `50k`, `50kb`, `10m`, `10mb`, `1g`, `1gb`.

## Default Operators

When you omit `op`, each scope uses a natural default:

| Scope | Default op | Why |
|---|---|---|
| `status` | `eq` | Status codes are exact matches |
| `body` | `eq` | Exact or schema match |
| `headers` | `eq` | Each header matched exactly |
| `size` | `eq` | Exact byte count |
| `redirects` | `eq` | URL equality |
| `totalDelayMs` | `lt` | "Must be faster than this" |
| `dns` | `lt` | Timing threshold |
| `connect` | `lt` | Timing threshold |
| `tls` | `lt` | Timing threshold |
| `ttfb` | `lt` | Timing threshold |
| `transfer` | `lt` | Timing threshold |
| `bodySize` | `lt` | Size threshold |

The available operators are: `lt`, `lte`, `eq`, `neq`, `gte`, `gt`.

## Body Matching

The `body` scope supports three kinds of matching:

### Schema validation

Validates the response JSON against a JSON Schema stored in a script variable:

```lace
.expect(body: schema($user_schema))
```

The variable must contain a valid JSON Schema document. If the variable is `null`, this is a **hard fail**.

**Schema match modes** control how extra fields are handled:

```lace
// Loose (default): schema fields must be present and valid, extra fields allowed
.expect(body: schema($user_schema))
.expect(body: { value: schema($user_schema), mode: "loose" })

// Strict: no extra fields allowed at any level
.expect(body: { value: schema($user_schema), mode: "strict" })
```

| Mode | Behaviour |
|---|---|
| `loose` (default) | All schema fields must be present and valid. Additional fields are ignored. |
| `strict` | All schema fields must be present and valid. Any extra field at any level fails. |

When a schema check fails, the assertion record includes the first validation error path and message (e.g. `{ path: ".user.id", detail: "expected integer, got string" }`).

### Literal string

The raw response body must match the string exactly:

```lace
.expect(body: "OK")
```

### Variable match

The raw response body must equal the runtime value of the variable:

```lace
.expect(body: $expected_body)
```

!!! note
    The `mode` field only applies to `schema(...)` body matching. Literal string and variable matches are always byte-exact; `mode` on those is silently ignored.

## Redirects Scope

The `redirects` scope asserts against the chain of redirect URLs followed during the call. It uses a `match` field to select which redirect to compare:

```lace
// Shorthand --- defaults to match: "any"
.expect(redirects: "/login")

// Full form --- explicit match type
.expect(redirects: { value: "/login", match: "first" })
.expect(redirects: { value: "/final", match: "last" })
.expect(redirects: { value: "/somewhere", match: "any" })
```

| `match` | Behaviour |
|---|---|
| `first` | Compare against the first redirect hop. Fails if no redirects occurred. |
| `last` | Compare against the last redirect hop. Fails if no redirects occurred. |
| `any` (default) | Pass if the value matches any redirect in the chain. |

## Combining .expect() and .check()

A common pattern: use `.expect()` for must-not-break thresholds and `.check()` for performance goals:

```lace
get("$BASE_URL/api/search")
.expect(
  status: 200,
  totalDelayMs: 10000     // hard fail: over 10 seconds is broken
)
.check(
  totalDelayMs: 1000,     // soft fail: over 1 second is slow
  dns: 50,
  tls: 100
)
```
