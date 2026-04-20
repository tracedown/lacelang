# Response Metadata

Each response record includes structured metadata for DNS resolution, TLS session
details, redirect tracking, and a full timing breakdown.

---

## DNS metadata

The `dns` object is always present on a response record. It captures the addresses
resolved for the call's hostname.

| Field | Type | Description |
|---|---|---|
| `resolvedIps` | `array of strings` | All addresses returned by the OS resolver, in order. Never omitted; at minimum a single-element array. |
| `resolvedIp` | `string \| null` | The address the executor actually connected to. Typically `resolvedIps[0]`, but may differ if the executor iterated through addresses after a connect failure. |

```json
{
  "resolvedIps": ["93.184.216.34", "2606:2800:220:1:248:1893:25c8:1946"],
  "resolvedIp": "93.184.216.34"
}
```

The executor populates these fields and nothing more. It performs no interpretation
(no geolocation, no blocklist checks) -- that is extension territory.

---

## TLS metadata

The `tls` object is present for HTTPS calls and `null` for plain HTTP.

| Field | Type | Description |
|---|---|---|
| `protocol` | `string` | Negotiated TLS protocol version, e.g. `"TLSv1.2"`, `"TLSv1.3"`. |
| `cipher` | `string` | Negotiated cipher suite name, e.g. `"TLS_AES_256_GCM_SHA384"`. |
| `alpn` | `string \| null` | Negotiated ALPN protocol (e.g. `"h2"`, `"http/1.1"`) or `null` if no ALPN was negotiated. |
| `certificate` | `object \| null` | Peer certificate metadata, or `null` when the runtime cannot expose it. |

### TLS example

=== "Compact"

    ```json
    {
      "protocol": "TLSv1.3",
      "cipher": "TLS_AES_256_GCM_SHA384",
      "alpn": "h2"
    }
    ```

=== "Full"

    ```json
    {
      "protocol": "TLSv1.3",
      "cipher": "TLS_AES_256_GCM_SHA384",
      "alpn": "h2",
      "certificate": {
        "subject": {
          "cn": "api.example.com"
        },
        "subjectAltNames": [
          "DNS:api.example.com",
          "DNS:www.example.com"
        ],
        "issuer": {
          "cn": "R3"
        },
        "notBefore": "2024-01-01T00:00:00Z",
        "notAfter": "2024-07-01T00:00:00Z"
      }
    }
    ```

### Certificate fields

When `certificate` is not `null`, it contains:

| Field | Type | Description |
|---|---|---|
| `subject` | `object` | Subject distinguished name. Always includes `cn` (common name). May include additional fields. |
| `subjectAltNames` | `array of strings` | SAN entries as `DNS:name` or `IP:addr` tokens. |
| `issuer` | `object` | Issuer distinguished name. Always includes `cn`. May include additional fields. |
| `notBefore` | `string` (ISO 8601) | Certificate validity start (UTC). |
| `notAfter` | `string` (ISO 8601) | Certificate validity end (UTC). |

### When certificate is null

When the executor is configured with `rejectInvalidCerts: false`, some runtimes cannot
expose the parsed certificate. In that case `certificate` is `null`, but `protocol`,
`cipher`, and `alpn` are still populated. The `tls` object itself is never omitted for
HTTPS calls -- only `certificate` within it may be `null`.

### Plain HTTP

For plain HTTP calls, `tls` is `null` and `tlsMs` is `0`:

=== "Compact"

    ```json
    {
      "status": 200,
      "statusText": "OK",
      "bodyPath": "/probe_runs/abc/call_0_response.txt",
      "responseTimeMs": 85,
      "tls": null
    }
    ```

=== "Full"

    ```json
    {
      "status": 200,
      "statusText": "OK",
      "headers": {
        "content-type": "text/plain"
      },
      "bodyPath": "/probe_runs/abc/call_0_response.txt",
      "responseTimeMs": 85,
      "dnsMs": 8,
      "connectMs": 22,
      "tlsMs": 0,
      "ttfbMs": 70,
      "transferMs": 15,
      "sizeBytes": 128,
      "dns": {
        "resolvedIps": [
          "10.0.0.5"
        ],
        "resolvedIp": "10.0.0.5"
      },
      "tls": null
    }
    ```

---

## Redirect tracking

The `redirects` field on each call record is an ordered array of URLs followed during
the request. It is an empty array when the call issued no redirects.

```json
{
  "redirects": [
    "https://example.com/old-path",
    "https://example.com/new-path",
    "https://example.com/final"
  ]
}
```

The array is populated even when a `REDIRECTS_MAX_LIMIT` hard-fail occurs, so you
can inspect the chain that led to the failure.

Redirect following is controlled by the call config:

```json
{
  "config": {
    "redirects": { "follow": true, "max": 10 }
  }
}
```

When `follow` is `false`, no redirects are followed and the array is always empty.

---

## Timing breakdown

Every response record includes a set of timing fields that break down the total
response time into phases. All values are integers in milliseconds.

| Field | Description |
|---|---|
| `responseTimeMs` | Total response time from request start to response complete. |
| `dnsMs` | Time spent on DNS resolution. |
| `connectMs` | Time spent establishing the TCP connection. |
| `tlsMs` | Time spent on TLS handshake. `0` for non-HTTPS calls. |
| `ttfbMs` | Time to first byte -- from sending the request to receiving the first byte of the response. |
| `transferMs` | Time spent transferring the response body. |
| `sizeBytes` | Total response body size in bytes. |

### Timing example

```json
{
  "responseTimeMs": 145,
  "dnsMs": 12,
  "connectMs": 34,
  "tlsMs": 28,
  "ttfbMs": 98,
  "transferMs": 47,
  "sizeBytes": 1024
}
```

These timing fields are also available as assertion scopes in the Lace language (e.g.
`.expect(dns: 100)` asserts that DNS resolution took at most 100ms).
