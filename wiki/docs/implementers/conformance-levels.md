# Conformance Levels

Not every executor implementation needs every feature of the spec. Some targets -- embedded probe runners, CI gates, bespoke monitoring appliances -- have no use for the extension system or no surface for emitting writeback actions. Lace defines conformance levels so such implementations can declare what they support and have that claim verified by the testkit.

Spec version: 0.9.1 (source: `lace-spec.md` section 17)

## Omissions

Two optional omissions may be declared:

| Omission | Meaning |
|---|---|
| `extensions` | The executor does not implement the `.laceext` processor. `.laceext` files are never loaded, no hook dispatch occurs, `require` is not enforced, and the `EXT_*` error codes are not emitted. |
| `actions` | The executor does not emit the `result.actions` object. `.store()` with `$$name` keys still works (lands in `runVars`); `$name` and plain-key writebacks have no observable side effect. The rest of the result structure is unchanged. |

No other axis is omissible at the spec level. TLS handshake scenarios, cookie jar modes, body storage, prev-access, and every other core feature are mandatory for any non-omitted level.

## Declaring the Level

### In the Executor Manifest

In `lace-executor.toml`:

```toml
[conformance]
omit = ["extensions"]         # or ["actions"], or both, or [] for full
```

Absent or empty `omit` means full conformance.

### On the Testkit CLI

Overrides the manifest when present:

```bash
lace-conformance -m ./lace-executor.toml --omit extensions,actions
```

## Testkit Behaviour

Each conformance vector carries an optional `requires` array listing the features it exercises:

```json
{ "id": "writeback_appears_in_actions_variables",
  "type": "execute",
  "requires": ["actions"],
  ... }
```

A vector whose `requires` list intersects the active omit set is **skipped** (outcome: `skipped`, reason: `"omitted: <feature>"`). Vectors without a `requires` list are always run.

## Outcome Labels

The testkit reports a conformance label at the end of each run:

| Situation | Label |
|---|---|
| No `omit`; every runnable vector passed | `compliant` |
| Non-empty `omit`; every runnable vector passed | `compliant-partial (omit: <features>)` |
| Any level; one or more in-scope failures | `non-compliant` |

Process exit code: `0` for `compliant` and `compliant-partial`, `1` for `non-compliant`. A CI gate that requires full compliance enforces that separately (e.g. `--require-level=full`, to be added when actually needed).

## What Partial Conformance Does Not Mean

- A `compliant-partial` executor is **not** spec-incompatible. It is spec-compliant at its declared level. Host platforms that do not use extensions can legitimately claim `compliant-partial (omit: extensions)` with no stigma.

- Omitting a feature removes both the obligation to implement it and the right to emit its artefacts. An executor claiming `omit: actions` must not emit a `result.actions` field in some runs and not others -- it is a structural guarantee, not a runtime toggle.

- `omit` does **not** affect the CLI contract: `parse` / `validate` / `run` subcommands, exit codes, stdout shape, and timeout handling are identical across all levels.
