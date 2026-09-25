# Body Storage

When `result.bodies.dir` is set to a path, Lace executors write response bodies to that
directory. The result JSON contains **absolute paths** to these files -- no body
bytes appear in the result JSON itself. Only response bodies are saved: request bodies
are never written to disk (they are already present in the script and its AST).

By default, body saving is **disabled** (`result.bodies.dir = false`). Enable it in
`lace.config` or with the `--save-body` / `--bodies-dir` CLI flags.

---

## Path convention

Body files follow the naming pattern:

```
{bodies_dir}/call_{index}_response.{ext}
```

| Segment | Description |
|---|---|
| `bodies_dir` | The body storage directory: `result.bodies.dir` in `lace.config`, or the directory set by `--save-body` / `--bodies-dir`. |
| `index` | Zero-based call index matching `calls[n].index`. |
| `ext` | File extension derived from the content type (e.g. `json`, `xml`, `txt`, `html`). |

### Examples

```
/var/lace/bodies/call_0_response.json
/var/lace/bodies/call_1_response.html
```

---

## Response bodyPath

Every response record includes a `bodyPath` field -- it is always present.

**Response `bodyPath`:** absolute path to the response body file when body saving is
enabled, or `null` otherwise (with `bodyNotCapturedReason` saying why).

```json
{
  "response": {
    "status": 200,
    "statusText": "OK",
    "headers": {
      "content-type": "application/json"
    },
    "bodyPath": "/var/lace/bodies/call_0_response.json",
    "responseTimeMs": 145
  }
}
```

---

## bodyNotCapturedReason

When the response `bodyPath` is `null`, the `bodyNotCapturedReason` field explains why.

| Value | Meaning |
|---|---|
| `"bodyTooLarge"` | The response body exceeded the call's `bodySize` scope value and was not written. The `bodySize` assertion records the failure; the call's `error` stays `null`. |
| `"notRequested"` | Body saving is disabled (`result.bodies.dir = false`, the default). |
| `"timeout"` | The call timed out before the body could be fully received. |

=== "Compact"

    ```json
    {
      "response": {
        "status": 200,
        "bodyPath": null,
        "bodyNotCapturedReason": "bodyTooLarge",
        "sizeBytes": 52428800
      }
    }
    ```

=== "Full"

    ```json
    {
      "response": {
        "status": 200,
        "statusText": "OK",
        "headers": {
          "content-type": "application/octet-stream"
        },
        "bodyPath": null,
        "bodyNotCapturedReason": "bodyTooLarge",
        "responseTimeMs": 320,
        "dnsMs": 5,
        "connectMs": 18,
        "tlsMs": 0,
        "ttfbMs": 100,
        "transferMs": 220,
        "sizeBytes": 52428800,
        "dns": {
          "resolvedIps": [
            "10.0.0.1"
          ],
          "resolvedIp": "10.0.0.1"
        },
        "tls": null
      }
    }
    ```

---

## Configuration

Body saving is controlled by `result.bodies.dir` in `lace.config`:

```toml
[result.bodies]
dir = "./lace_results/bodies"    # path string = save here, false = don't save
```

| Config key | Default | Description |
|---|---|---|
| `result.bodies.dir` | `false` | Path string: save body files to this directory. `false`: do not save. |

The `--save-body` CLI flag sets `result.bodies.dir` to the result path (or system temp) for a single run.
The `--bodies-dir <path>` CLI flag sets `result.bodies.dir` to the given path explicitly.
