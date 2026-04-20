# Result Schema

The `ProbeResult` is the wire format returned by every Lace executor. Conforms to `lace-spec.md` section 9. Extension-defined fields appear under `actions.{key}` (arrays) and `runVars` (scalars with `{extension_name}.` prefix).

Spec version: 0.9.0

## Top-Level Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `outcome` | `"success"` \| `"failure"` \| `"timeout"` | Yes | Overall run outcome. |
| `startedAt` | string (ISO 8601 UTC) | Yes | Timestamp before the first call begins. |
| `endedAt` | string (ISO 8601 UTC) | Yes | Timestamp after all chain methods complete or after cascade stops. |
| `elapsedMs` | integer (>= 0) | Yes | Wall-clock elapsed time in milliseconds. |
| `runVars` | object | Yes | Final state of all `$$var` assignments plus extension-emitted variables. Each `$$var` key appears at most once (write-once rule). |
| `calls` | array of [CallRecord](#callrecord) | Yes | Ordered call records including skipped calls. |
| `actions` | object | Yes | Free-form action map. `actions.variables` is the only typed mandatory section (writeback `.store()` entries). Other keys are extension-defined. |

### actions.variables

| Field | Type | Description |
|---|---|---|
| `variables` | object | Writeback variables from `.store()` (`$name` and plain keys). The `$` prefix is stripped. Values may be any JSON-serialisable shape. |

## CallRecord

| Field | Type | Required | Description |
|---|---|---|---|
| `index` | integer (>= 0) | Yes | Zero-based call index in script order. |
| `outcome` | `"success"` \| `"failure"` \| `"timeout"` \| `"skipped"` | Yes | |
| `startedAt` | string (ISO 8601) \| null | Yes | Null for skipped calls. |
| `endedAt` | string (ISO 8601) \| null | Yes | Null for skipped calls. |
| `request` | [RequestRecord](#requestrecord) \| null | Yes | Null for skipped calls. |
| `response` | [ResponseRecord](#responserecord) \| null | Yes | Null for skipped, timeout (no response received), or connection failure. |
| `redirects` | array of string | Yes | Ordered URLs followed. Empty array when no redirects. Populated even on `REDIRECTS_MAX_LIMIT` hard-fail. |
| `assertions` | array of [ScopeAssertion](#scopeassertion) or [ConditionAssertion](#conditionassertion) | Yes | All evaluated scopes and conditions in evaluation order. Empty for skipped calls. |
| `config` | object | Yes | Resolved call config (after defaults applied), including extension-registered fields. |
| `warnings` | array of string | Yes | Warning strings (null interpolations, TLS warnings, etc.). Empty array if none. |
| `error` | string \| null | Yes | Non-assertion failure detail (connection error, TLS, redirect limit, body too large). Null otherwise. |

## RequestRecord

| Field | Type | Required | Description |
|---|---|---|---|
| `url` | string | Yes | Resolved URL after variable interpolation. |
| `method` | `"get"` \| `"post"` \| `"put"` \| `"patch"` \| `"delete"` | Yes | |
| `headers` | object (string values) | Yes | Resolved headers sent on the wire. |
| `bodyPath` | string \| null | Yes | Absolute path to request body file on shared storage. Null if no body. |

## ResponseRecord

| Field | Type | Required | Description |
|---|---|---|---|
| `status` | integer (100-599) | Yes | HTTP status code. |
| `statusText` | string | Yes | HTTP status text. |
| `headers` | object (string or string[] values) | Yes | Lower-cased header names. Multi-value headers as string arrays. |
| `bodyPath` | string \| null | Yes | Absolute path to response body file. Null when not captured. |
| `bodyNotCapturedReason` | `"bodyTooLarge"` \| `"notRequested"` \| `"timeout"` | No | Present when `bodyPath` is null. |
| `responseTimeMs` | integer (>= 0) | Yes | Total response time in ms. |
| `dnsMs` | integer (>= 0) | Yes | DNS resolution time in ms. |
| `connectMs` | integer (>= 0) | Yes | TCP connection time in ms. |
| `tlsMs` | integer (>= 0) | Yes | TLS handshake time in ms. 0 for non-HTTPS calls. |
| `ttfbMs` | integer (>= 0) | Yes | Time to first byte in ms. |
| `transferMs` | integer (>= 0) | Yes | Body transfer time in ms. |
| `sizeBytes` | integer (>= 0) | Yes | Response body size in bytes. |
| `dns` | [DnsMeta](#dnsmeta) | Yes | DNS resolution metadata. |
| `tls` | [TlsMeta](#tlsmeta) \| null | Yes | TLS metadata for HTTPS calls; null for plain HTTP. |

## DnsMeta

DNS resolution metadata. The core executor populates these fields; extensions may interpret them.

| Field | Type | Required | Description |
|---|---|---|---|
| `resolvedIps` | array of string | Yes | All addresses returned by the OS resolver, in order. |
| `resolvedIp` | string \| null | Yes | The address the executor actually connected to. Typically `resolvedIps[0]`. |

## TlsMeta

TLS session metadata. Null when `rejectInvalidCerts: false` leaves the certificate unparseable, but the rest of the object is always populated.

| Field | Type | Required | Description |
|---|---|---|---|
| `protocol` | string | Yes | Negotiated TLS protocol version (e.g. `"TLSv1.3"`). |
| `cipher` | string | Yes | Negotiated cipher suite name. |
| `alpn` | string \| null | Yes | Negotiated ALPN protocol (e.g. `"h2"`) or null. |
| `certificate` | [CertificateMeta](#certificatemeta) \| null | Yes | Peer certificate. Null when the runtime cannot expose it. |

## CertificateMeta

| Field | Type | Required | Description |
|---|---|---|---|
| `subject` | object (`cn`: string, plus additional fields) | Yes | Certificate subject. |
| `subjectAltNames` | array of string | Yes | SAN entries as `DNS:name` / `IP:addr` tokens. |
| `issuer` | object (`cn`: string, plus additional fields) | Yes | Certificate issuer. |
| `notBefore` | string (ISO 8601 UTC) | Yes | Certificate validity start. |
| `notAfter` | string (ISO 8601 UTC) | Yes | Certificate validity end. |

## ScopeAssertion

Assertion record for `.expect()` and `.check()` scopes.

| Field | Type | Required | Description |
|---|---|---|---|
| `method` | `"expect"` \| `"check"` | Yes | Which chain method produced this assertion. |
| `scope` | string | Yes | Scope name: `status`, `body`, `headers`, `bodySize`, `totalDelayMs`, `dns`, `connect`, `tls`, `ttfb`, `transfer`, `size`, `redirects`. |
| `op` | `"lt"` \| `"lte"` \| `"eq"` \| `"neq"` \| `"gte"` \| `"gt"` | Yes | Comparison operator used. |
| `match` | `"first"` \| `"last"` \| `"any"` | No | Only present on `redirects` scope assertions. Selects which redirect URL is compared. |
| `outcome` | `"passed"` \| `"failed"` \| `"indeterminate"` | Yes | |
| `actual` | any | Yes | Observed value. Type depends on scope. |
| `expected` | any | Yes | Expected value as written in source (after variable resolution). |
| `options` | object \| null | Yes | The `options {}` object from source, passed through opaquely. Null if no options block. |

## ConditionAssertion

Assertion record for `.assert()` conditions.

| Field | Type | Required | Description |
|---|---|---|---|
| `method` | `"assert"` | Yes | Always `"assert"`. |
| `kind` | `"expect"` \| `"check"` | Yes | Whether the condition is hard-fail or soft-fail. |
| `index` | integer (>= 0) | Yes | Index within the assert array. |
| `outcome` | `"passed"` \| `"failed"` \| `"indeterminate"` | Yes | |
| `expression` | string | Yes | Expression as written in source. |
| `actualLhs` | any | Yes | Resolved left operand value. |
| `actualRhs` | any | Yes | Resolved right operand value. |
| `options` | object \| null | Yes | The `options {}` object from source. Null if none. |
