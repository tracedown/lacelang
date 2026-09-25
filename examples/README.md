# Examples

Runnable `.lace` scripts with the real `result.json` the Python reference
executor produced for them. Each directory holds `script.lace`, optionally
`vars.json` / `lace.config`, and `result.json`.

The results are generated with the default config — no body saving, so
`bodyPath` is `null` with `bodyNotCapturedReason: "notRequested"` — and
then anonymised: resolved IPs become `203.0.113.x`, timestamps and
certificate dates become the Unix epoch, volatile headers are dropped.

Regenerate after an executor change:

```bash
for d in examples/*/; do
  ( cd "$d"
    extra=""; [ -f vars.json ] && extra="--vars vars.json"
    lacelang-executor run script.lace $extra --save-to result.json --pretty )
done
```

then anonymise as above and re-paste the compacted snippets into
`README.md` (§Examples), `wiki/docs/index.md` and
`wiki/docs/getting-started/examples.md`.
