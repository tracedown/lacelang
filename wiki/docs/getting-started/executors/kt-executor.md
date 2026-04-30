# Kotlin Executor

Reference Kotlin/JVM implementation of Lace, conformant to spec version
**0.9.1** (178/178 conformance vectors). Passes the same test suite as the
canonical Python executor and is fully interchangeable.

The implementation is split into two packages following the
[packaging rule](../../implementers/packaging.md):

| Package | Repository | Description |
|---|---|---|
| `lacelang-kt-validator` | [tracedown/lacelang-kotlin-validator](https://github.com/tracedown/lacelang-kotlin-validator) | Lexer, parser, semantic validator. Single dependency (Gson). |
| `lacelang-kt-executor` | [tracedown/lacelang-kotlin-executor](https://github.com/tracedown/lacelang-kotlin-executor) | HTTP runtime, assertion evaluation, cookie jars, extension dispatch. Depends on the validator. |

Requires JDK **17+**.

---

## Installation

### From GitHub Releases

```bash
curl -sL "https://github.com/tracedown/lacelang-kotlin-executor/releases/latest/download/lacelang-kt-executor-0.1.0-all.jar" \
    -o lacelang-kt-executor.jar

java -jar lacelang-kt-executor.jar --version
```

The shadow JAR bundles the validator — a single JAR is all you need.

### From source

```bash
git clone https://github.com/tracedown/lacelang-kotlin-validator.git
git clone https://github.com/tracedown/lacelang-kotlin-executor.git
cd lacelang-kotlin-executor
./gradlew shadowJar
```

The executor uses a Gradle
[composite build](https://docs.gradle.org/current/userguide/composite_builds.html)
to resolve the validator. Both repos must be siblings on disk.

---

## CLI usage

The executor CLI exposes three subcommands matching the
[testkit contract](../../implementers/index.md):

### `parse` — syntax check

```bash
java -jar lacelang-kt-executor.jar parse script.lace
```

Outputs the AST as JSON. Parsing is delegated to `lacelang-kt-validator`.

### `validate` — semantic checks

```bash
java -jar lacelang-kt-executor.jar validate script.lace \
    --vars-list vars.json \
    --context context.json
```

Runs the parser and semantic validator. Reports structured errors and
warnings. No HTTP calls are made.

### `run` — full execution

```bash
java -jar lacelang-kt-executor.jar run script.lace \
    --vars vars.json \
    --prev prev.json \
    --pretty
```

Parses, validates, executes, and emits a
[ProbeResult](../../result/index.md) JSON.

#### Run flags

| Flag | Description |
|---|---|
| `--vars <file>` | JSON object with script variable values (`$var`). |
| `--var KEY=VALUE` | Inject a single variable (repeatable, overrides `--vars`). VALUE is parsed as JSON when valid, otherwise kept as a string. |
| `--prev-results <file>` | Previous result JSON, making `prev` available in expressions. `--prev` is a short alias. |
| `--config <file>` | Explicit path to a `lace.config` TOML file. |
| `--env <name>` | Select `[lace.config.<name>]` section (overrides `LACE_ENV`). |
| `--enable-extension <name>` | Activate a built-in extension (repeatable). |
| `--save-to <path>` | Persist the result to disk. Directory: timestamped JSON. File: overwrite. `"false"`: skip. |
| `--bodies-dir <path>` | Directory for request/response body files. |
| `--save-body` | Enable response body file writing. |
| `--pretty` | Pretty-print the result JSON. |

---

## Library API

The primary interface is the CLI. The `runScript()` function is available
for embedding in Kotlin/JVM applications:

```kotlin
import dev.lacelang.executor.runScript
import dev.lacelang.executor.loadConfig
import dev.lacelang.validator.parse

val ast = parse("""get("https://example.com").expect(status: 200)""")
val config = loadConfig()
val result = runScript(
    ast,
    scriptVars = mapOf("key" to "val"),
    config = config,
)
// result is Map<String, Any?> matching the ProbeResult wire format
```

---

## Configuration

There is exactly **one config file** per executor. The `env` parameter
selects a **section within that file**, not a different file.

```toml
# lace/lace.config

[executor]
maxRedirects = 10
maxTimeoutMs = 300000

# Staging overlay — deep-merged on top of base
[lace.config.staging]
[lace.config.staging.executor]
maxTimeoutMs = 60000
```

See the [Configuration](../../language/configuration.md) page for details.

---

## Return value

`runScript()` returns a `Map<String, Any?>` matching the
[ProbeResult wire format](../../result/index.md):

```json
{
  "outcome": "success",
  "startedAt": "2026-04-30T10:00:00.000Z",
  "endedAt": "2026-04-30T10:00:01.234Z",
  "elapsedMs": 1234,
  "runVars": {},
  "calls": [],
  "actions": {}
}
```

---

## User-Agent

Per spec [section 3.6](../../language/http-calls.md), this executor sets a
default `User-Agent` on outgoing requests:

```
User-Agent: lace-probe/<version> (lacelang-kotlin)
```

Precedence (highest first): per-request `headers: { "User-Agent": ... }` —
`lace.config [executor].user_agent` — the default above.
