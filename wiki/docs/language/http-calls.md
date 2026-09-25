# HTTP Calls

Every Lace script is built from HTTP calls. A call is a method name, a URL, an optional config object, and one or more chain methods.

## Methods

Lace supports five HTTP methods:

```lace
get(url, config?)
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

There is no escape that suppresses interpolation. The string escape `\$` yields a `$` character, but interpolation runs on the resulting text, so `"\$name"` still interpolates when `name` is a variable. Braced forms only control where a reference ends. A missing variable interpolates as `null` (see [Null Semantics](variables.md#null-semantics)).

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

A `max` above the system maximum (`executor.maxRedirects`) is a validation error (`REDIRECTS_MAX_LIMIT`). A call that needs more than `max` redirect hops at runtime is a hard fail. The hops actually followed are recorded -- see [Redirect Tracking](#redirect-tracking).

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

An `ms` above the system maximum (`executor.maxTimeoutMs`) is a validation error (`TIMEOUT_MS_LIMIT`).

**Timeout actions:**

| Action | Behaviour |
|---|---|
| `"fail"` | Hard fail immediately |
| `"warn"` | Record a soft fail, continue execution |
| `"retry"` | Retry up to `retries` times, then hard fail |

!!! note
    `retries` is only valid when `action` is `"retry"`. Using it with other actions is a validation error.

### Extension fields

Extensions may register additional fields on the call config root and on the `redirects`, `security`, and `timeout` objects (for example, `laceNotifications` registers `timeout.notification`). They are recorded in the call record's resolved `config` under an `extensions` sub-object of the owning object -- `timeout.notification` appears as `config.timeout.extensions.notification`, a root-level field as `config.extensions.<name>`.

## Cookie Jar Modes

Cookie jars control how cookies persist between calls in a script. When `cookieJar` is omitted, the call uses `"inherit"`.

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
2. `[executor].user_agent` in `lace.config`
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

### DNS metadata (`this.dns`)

```json
{
  "resolvedIps": ["93.184.216.34"],
  "resolvedIp":  "93.184.216.34"
}
```

| Field | Description |
|---|---|
| `resolvedIps` | Every address returned by resolution, in the order the resolver gave them. Never omitted; at least one element. |
| `resolvedIp` | The address the executor actually connected to. Equal to `resolvedIps[0]` unless the executor moved on to another address after a connect failure. |

### TLS metadata (`this.tls`)

For HTTPS calls `this.tls` is an object; for plain HTTP it is `null`.

```json
{
  "protocol": "TLSv1.3",
  "cipher":   "TLS_AES_256_GCM_SHA384",
  "alpn":     "h2",
  "certificate": {
    "subject":         { "cn": "example.com" },
    "subjectAltNames": ["DNS:example.com", "DNS:www.example.com"],
    "issuer":          { "cn": "R3" },
    "notBefore":       "2026-01-01T00:00:00Z",
    "notAfter":        "2026-04-01T00:00:00Z"
  }
}
```

| Field | Description |
|---|---|
| `protocol` | Negotiated TLS version, e.g. `"TLSv1.2"`, `"TLSv1.3"`. |
| `cipher` | Negotiated cipher suite name. |
| `alpn` | Negotiated ALPN protocol (e.g. `"h2"`, `"http/1.1"`), or `null` if none. |
| `certificate` | Peer certificate metadata, or `null`. Contains `subject.cn`, `subjectAltNames` (`DNS:…` / `IP:…` tokens), `issuer.cn`, and `notBefore` / `notAfter` (ISO-8601 UTC). Under `rejectInvalidCerts: false` some runtimes cannot expose the certificate -- they still populate `protocol`, `cipher`, and `alpn` and set `certificate` to `null`. |

The executor only surfaces DNS and TLS metadata; it does not interpret it. Policy checks such as certificate pinning, cipher allow-lists, or IP blocklists belong to [extensions](../extensions/index.md) or `.assert()` conditions:

```lace
get("https://api.example.com/health")
.expect(status: 200)
.assert({
  check: [
    this.tls.protocol eq "TLSv1.3",
    this.dns.resolvedIp neq null
  ]
})
```

## Redirect Tracking

Every call records the URLs it was redirected to, in order, on `this.redirects` and on `calls[n].redirects` in the result.

- Each entry is the resolved URL of a redirect hop that was actually issued. The initial request URL is not included (it is `calls[n].request.url`), and neither is the final URL unless the final response was itself a redirect.
- The list is `[]` when the call was not redirected, including when `redirects.follow` is `false`.
- When a call hard-fails because it needed more than `redirects.max` hops, the list still shows the hops up to and including the one that hit the limit.

For example, `get("/a")` receiving `302 → /b`, then `302 → /c`, then `200` at `/c` records `["/b", "/c"]`.

Assert on redirect hops with the [`redirects` scope](assertions.md#redirects-scope) and its `match` field (`first`, `last`, or `any`):

```lace
get("$BASE_URL/account")
.expect(redirects: { value: "$BASE_URL/login", match: "first" })
```
