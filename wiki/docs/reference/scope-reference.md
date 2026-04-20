# Scope Reference

Scopes are the assertion targets used in `.expect()` and `.check()` chain methods. Each scope maps to a specific property of the HTTP response.

Spec version: 0.9.0

## Scope Table

| Scope | Default Op | Value Type | Description | Notes |
|---|---|---|---|---|
| `status` | `eq` | integer or integer[] | HTTP status code. Array form passes when actual matches any element. | |
| `body` | `eq` | body match | Response body. Accepts `schema($var)`, string literal, or variable ref. See [body matching](../language/assertions.md#body-matching). | |
| `headers` | `eq` | object | Each key-value must match. Header keys are case-insensitive. | |
| `bodySize` | `lt` | size string | Body size threshold (e.g. `"50k"`, `"10mb"`). Also gates body capture -- exceeding this value sets `bodyNotCapturedReason: "bodyTooLarge"`. | |
| `totalDelayMs` | `lt` | integer | Total response time (`this.responseTime`) threshold in ms. | |
| `dns` | `lt` | integer | DNS resolution time (`this.dnsMs`) threshold in ms. | |
| `connect` | `lt` | integer | TCP connection time (`this.connect`) threshold in ms. | |
| `tls` | `lt` | integer | TLS handshake time (`this.tlsMs`) threshold in ms. | Skipped (not evaluated) when `this.tlsMs eq 0` (plain HTTP). |
| `ttfb` | `lt` | integer | Time to first byte (`this.ttfb`) threshold in ms. | |
| `transfer` | `lt` | integer | Body transfer time (`this.transfer`) threshold in ms. | |
| `size` | `eq` | integer | Exact response body size (`this.size`) in bytes. Use `bodySize` for threshold checks. | |
| `redirects` | `eq` | string | Assertion over redirect URLs (`this.redirects`). Uses a `match` field to select which hop. | Only scope that supports the `match` modifier. |

## Scope Forms

Every scope accepts three syntactic forms:

**Shorthand** -- bare value, default op, no options:

```lace
totalDelayMs: 200
status: [200, 201]
```

**Value + op** -- explicit comparison operator:

```lace
totalDelayMs: { value: 500, op: "lte" }
```

**Full form** -- value, optional op, and options block:

```lace
status: {
  value: 200,
  op: "eq",
  options: { ... }
}
```

## Redirect Match Modes

The `redirects` scope supports a `match` field:

| Match | Behaviour |
|---|---|
| `first` | Compare value to `this.redirects[0]`. Fails if array is empty. |
| `last` | Compare value to `this.redirects[-1]`. Fails if array is empty. |
| `any` | Pass if value equals any element of `this.redirects`. |

Default: `any`. Only `eq` is a meaningful op for URLs.

## Body Schema Match Modes

The `body` scope with `schema($var)` supports a `mode` field:

| Mode | Behaviour |
|---|---|
| `loose` (default) | Schema fields all present and type-valid; extra fields accepted. |
| `strict` | Schema fields all present and type-valid; no extra fields at any level. |

## Default Operator Rationale

- **Exact match scopes** (`status`, `body`, `headers`, `size`, `redirects`): default `eq` because these are identity checks.
- **Threshold scopes** (`totalDelayMs`, `dns`, `connect`, `tls`, `ttfb`, `transfer`, `bodySize`): default `lt` because the common pattern is "response time must be less than N milliseconds."
