# HTTP Calls

Every Lace script is built from HTTP calls. A call is a method name, a URL, an optional config object, and one or more chain methods.

## Methods

Lace supports five HTTP methods:

```lace
get(url)
post(url, config?)
put(url, config?)
patch(url, config?)
delete(url, config?)
```

`HEAD` is not supported --- Lace probes require a response body.

## URL and Variable Interpolation

The first argument is always a quoted string. Variables are interpolated directly:

```lace
get("$BASE_URL/api/users")
get("$BASE_URL/api/users/$$user_id/posts")
```

Use `${$var}` or `${$$var}` when you need to disambiguate from surrounding text:

```lace
get("$BASE_URL/api/${$resource_type}s")
```

Escape a literal dollar sign with `\$`.

## Request Config

The second argument is an optional object with these fields (all optional):

```lace
post("$BASE_URL/login", {
  headers:  { "X-Request-ID": "$request_id", Authorization: "Bearer $token" },
  body:     json({ email: "$admin_email", password: "$admin_pass" }),
  cookies:  { locale: "en" },
  cookieJar: "named:admin",

  redirects: {
    follow: true,
    max: 10
  },

  security: {
    rejectInvalidCerts: false
  },

  timeout: {
    ms:      5000,
    action:  "retry",
    retries: 2
  }
})
.expect(status: 200)
```

### headers

An object of header name-value pairs. Values support variable interpolation.

### body

Three forms:

```lace
// JSON body (sets Content-Type: application/json)
body: json({ email: "$email", password: "$pass" })

// URL-encoded form (sets Content-Type: application/x-www-form-urlencoded)
body: form({ username: "$user", password: "$pass" })

// Raw string
body: "plain text body"
```

### cookies

An object of cookie name-value pairs sent with the request.

### cookieJar

Controls cookie persistence across calls. See [Cookie Jar Modes](#cookie-jar-modes) below.

### redirects

| Field | Default | Description |
|---|---|---|
| `follow` | `true` | Whether to follow redirects |
| `max` | `10` | Maximum number of redirects to follow |

Exceeding the system maximum for `max` is always a hard fail.

### security

| Field | Default | Description |
|---|---|---|
| `rejectInvalidCerts` | `true` | Reject invalid TLS certificates |

When set to `false`, TLS errors produce a warning but execution continues.

### timeout

| Field | Default | Description |
|---|---|---|
| `ms` | From execution context | Timeout in milliseconds |
| `action` | `"fail"` | What to do on timeout |
| `retries` | `0` | Number of retries (only valid with `action: "retry"`) |

**Timeout actions:**

| Action | Behaviour |
|---|---|
| `"fail"` | Hard fail immediately |
| `"warn"` | Record a soft fail, continue execution |
| `"retry"` | Retry up to `retries` times, then hard fail |

!!! note
    `retries` is only valid when `action` is `"retry"`. Using it with other actions is a validation error.

## Cookie Jar Modes

Cookie jars control how cookies persist between calls in a script.

| Mode | Description |
|---|---|
| `"inherit"` | Continue with the previous call's cookies. The first call starts with an empty jar. |
| `"fresh"` | Discard all cookies and start empty. |
| `"selective_clear"` | Remove specific cookies from the default jar, then continue. Requires `clearCookies`. |
| `"named:{name}"` | Use or create an isolated named jar. The name must be non-empty and alphanumeric. |
| `"{name}:selective_clear"` | Clear specific cookies from a named jar. Requires `clearCookies`. |

When using `selective_clear`, specify which cookies to remove with the `clearCookies` field:

```lace
get("$BASE_URL/page", {
  cookieJar: "selective_clear",
  clearCookies: ["session_id", "tracking"]
})
.expect(status: 200)
```

Named jars are isolated from each other and from the default jar. They persist for the duration of the run only.

```lace
// First call creates the "admin" jar
post("$BASE_URL/admin/login", {
  body: json({ email: "$admin_email", password: "$admin_pass" }),
  cookieJar: "named:admin"
})
.expect(status: 200)

// Second call reuses the "admin" jar (with its cookies)
get("$BASE_URL/admin/dashboard", {
  cookieJar: "named:admin"
})
.expect(status: 200)
```

## Default Headers

Every executor sets a `User-Agent` header automatically:

```
User-Agent: lace-probe/<executor-version> (<implementation-name>)
```

**Override precedence** (highest first):

1. `headers: { "User-Agent": "..." }` on the call
2. `user_agent` in `lace.config`
3. The default above

The executor sets no other implicit headers beyond what the HTTP stack requires (`Host`, `Content-Length`, `Content-Type` for `json()`/`form()` bodies).

## The Response Object (`this`)

Inside chain methods, `this` refers to the response from the current call. It is read-only.

| Field | Type | Description |
|---|---|---|
| `this.status` | integer | HTTP status code |
| `this.statusText` | string | HTTP status text |
| `this.body` | object or string | Parsed JSON or raw string |
| `this.headers` | object | Response headers (lower-cased keys) |
| `this.responseTime` | integer | Total response time in ms |
| `this.connect` | integer | TCP connection time in ms |
| `this.ttfb` | integer | Time to first byte in ms |
| `this.transfer` | integer | Body transfer time in ms |
| `this.size` | integer | Response body size in bytes |
| `this.redirects` | array | Ordered URLs of redirect hops (empty if none) |
| `this.dns` | object | DNS resolution metadata |
| `this.dnsMs` | integer | DNS resolution time in ms |
| `this.tls` | object or null | TLS metadata (`null` for plain HTTP) |
| `this.tlsMs` | integer | TLS handshake time in ms (0 for HTTP) |

!!! note
    In `.expect()` and `.check()` scopes, the shorthand names `dns` and `tls` refer to the timing values (`dnsMs` / `tlsMs`), not the metadata objects. Use `this.dns` and `this.tls` in `.assert()` expressions when you need the full objects.
