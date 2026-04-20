# Body Storage

Lace executors write request and response bodies to a shared filesystem volume. The
result JSON contains **absolute paths** to these files -- no body bytes appear in the
result JSON itself.

---

## Path convention

Body files follow the naming pattern:

```
{run_base_dir}/call_{index}_{request|response}.{ext}
```

| Segment | Description |
|---|---|
| `run_base_dir` | Base directory for this probe run. Configured via `result.bodies.dir` in `lace.config`. |
| `index` | Zero-based call index matching `calls[n].index`. |
| `request \| response` | Whether the file contains the request body or response body. |
| `ext` | File extension derived from the content type (e.g. `json`, `xml`, `txt`, `html`). |

### Examples

```
/probe_runs/abc/call_0_request.json
/probe_runs/abc/call_0_response.json
/probe_runs/abc/call_1_response.html
```

---

## bodyPath

Both the request record and the response record include a `bodyPath` field.

**Request `bodyPath`:** absolute path to the request body file, or `null` if no body
was sent (typical for GET and DELETE requests).

=== "POST"

    ```json
    {
      "request": {
        "url": "https://api.example.com/users",
        "method": "post",
        "headers": {
          "content-type": "application/json"
        },
        "bodyPath": "/probe_runs/abc/call_0_request.json"
      }
    }
    ```

=== "GET"

    ```json
    {
      "request": {
        "url": "https://api.example.com/users",
        "method": "get",
        "headers": {},
        "bodyPath": null
      }
    }
    ```

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
| `"notRequested"` | The probe configuration did not request body capture for this call. |
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

The run base directory is set in `lace.config`:

```toml
[result.bodies]
dir = "./lace_results/bodies"
```

If not specified, it defaults to the same directory as `result.path` (which itself
defaults to the current directory).

| Config key | Default | Description |
|---|---|---|
| `result.bodies.dir` | Same as `result.path` | Directory where body files are written. |
