# Result Additions

Extensions declare what they contribute to the run result under the `[result]` section of the `.laceext` file. This serves two purposes:

1. The validator can type-check `emit` statements in rules against the declared structure.
2. Downstream tooling (backends, schema validators) knows what shape to expect from extensions.

## Declaring result actions

Result action arrays are declared under `[result.actions.<key>]`:

```toml
[result.actions.notifications]
type = "array<notification_event>"
```

This declares that the extension will emit entries into `result.actions.notifications`. Each entry must conform to the `notification_event` type.

## Declaring result types

Custom types used by result actions are declared under `[result.types]`:

```toml
[result.types.notification_event.fields]
callIndex      = "int"
conditionIndex = "int"
trigger         = "string"
scope           = "string?"
notification    = "notification_val"
```

A result type is a field record only: a `fields` table mapping each field name to a type name (no `one_of` or aliases -- those belong in the top-level `[types]` section). Field types use the same type system as `[types]` (see [Schema Additions](schema-additions.md)) -- built-in types, custom types defined in `[types]` (like `notification_val` above), and the nullable shorthand `string?`.

## Full example

From `laceNotifications.laceext`:

```toml
# Declare the action array
[result.actions.notifications]
type = "array<notification_event>"

# Define the entry type
[result.types.notification_event.fields]
callIndex      = "int"
conditionIndex = "int"
trigger         = "string"
scope           = "string?"
notification    = "notification_val"
```

Rules then emit to this array:

```
emit result.actions.notifications <- {
  callIndex:      call.index,
  conditionIndex: -1,
  trigger:         "expect",
  scope:           scope.name,
  notification:    $notif
}
```

## Emit targets

The `emit` statement can target two kinds of paths:

| Target | Type | Behaviour |
|---|---|---|
| `result.actions.<key>` | array | Appends an object to the array. The array is initialised to `[]` if not yet present. |
| `result.runVars` | object | Merges a key-value pair into the `runVars` map. The key must be prefixed with the extension name. |

Attempting to emit to any other path (e.g. `result.calls`, `result.outcome`) is rejected: the emit is dropped, a warning (`EXT_EMIT_FORBIDDEN_TARGET`) is recorded in the call's `warnings` array, and execution continues. Core result fields (`calls`, `outcome`, `startedAt`, `endedAt`, ...) are never writable.

## Namespace rules for runVars

All keys emitted to `result.runVars` must be prefixed with the extension name followed by a dot:

```
emit result.runVars <- {
  "laceBaseline.stats": $new_stats
}
```

An emit where the key does not start with `{extension_name}.` is rejected the same way -- the emit is dropped and a warning (`EXT_RUN_VAR_NAMESPACE`) is recorded. This prefix prevents collisions between extensions and with script-author `$$var` entries (which are always bare identifiers).

See [Variables & Config](variables-and-config.md) for more on reading and writing extension variables.
