# Body Storage

When `result.bodies.dir` is set to a path, Lace executors write response bodies to that
directory. The result JSON contains **absolute paths** to these files -- no body
bytes appear in the result JSON itself.

By default, body saving is **disabled** (`result.bodies.dir = false`). Enable it in
`lace.config` or with the `--save-body` / `--bodies-dir` CLI flags.

---

## Path convention

Body files follow the naming pattern:

```
{run_base_dir}/call_{index}_response.{ext}
```

| Segment | Description |
|---|---|
| `run_base_dir` | Base directory for this probe run. Configured via `result.bodies.dir` in `lace.config`. |
| `index` | Zero-based call index matching `calls[n].index`. |
| `ext` | File extension derived from the content type (e.g. `json`, `xml`, `txt`, `html`). |

### Examples

```
/probe_runs/abc/call_0_response.json
/probe_runs/abc/call_1_response.html
```

---

## Response bodyPath

The response record includes a `bodyPath` field.

**Response `bodyPath`:** absolute path to the response body file, or `null` when the
body was not captured.

```json
{
  "response": {
    "status": 200,
    "statusText": "OK",
    "headers": {
      "content-type": "application/json"
    },
    "bodyPath": "/probe_runs/abc/call_0_response.json",
    "responseTimeMs": 145
  }
}
```

---

## bodyNotCapturedReason

When the response `bodyPath` is `null`, the `bodyNotCapturedReason` field explains why.

| Value | Meaning |
|---|---|
| `"bodyTooLarge"` | The response body exceeded the configured size limit and was not written. |
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
        "tlsMs": 25,
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
