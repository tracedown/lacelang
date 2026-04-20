# Block Editor Mapping

UI slot mapping for visual/block editors that read and write `.lace` source text via `lace-js-translator`.

Spec version: 0.9.0 (source: `lace-spec.md` section 13)

## Sub-Block Slots

The block editor renders chain methods as collapsible sub-blocks in a fixed order:

| Slot | Chain Method | Collapsed When |
|---|---|---|
| Expect | `.expect(...)` | No scopes |
| Check | `.check(...)` | No scopes |
| Store | `.store({...})` | No entries |
| Assert | `.assert({...})` | No conditions |
| Wait | `.wait(N)` | Not configured |

Execution parameters (`timeout`, `redirects`, `security`) are configured on the call config object in the request block UI, not as a separate chain method.

## Scope Entry Editor

Three modes toggled by the author:

1. **Shorthand** -- value only, default op inferred from scope name.
2. **Value + op** -- explicit comparison operator.
3. **Full form** -- shows the `options {}` panel when an extension is active. When no extension is active, the `options {}` panel is hidden.

## Options Panel

The `options {}` panel is rendered by the extension's block editor contribution. Extensions register UI components for their option fields alongside their `.laceext` schema definition.

## Variable Picker

The variable picker lists variables from the injected registry:

- **Grouping** is a host application concern.
- `$$` variables display a **run-scope** badge.
- `$` and plain keys display a **writeback** badge.
- Duplicate `$$var` assignments show an **error indicator**.
