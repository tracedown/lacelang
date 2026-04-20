# Schema Additions

Schema additions declare new fields that an extension registers on existing Lace objects. When the extension is active, the validator accepts these fields. When inactive, they produce unknown-field warnings.

## Registration targets

Each key under `[schema]` maps to a location in the Lace object model:

| Target key | Where the field appears |
|---|---|
| `scope_options` | The `options {}` block of any scope in `.expect()` / `.check()` |
| `condition_options` | The `options {}` block of any condition in `.assert()` |
| `timeout` | The `timeout {}` call config sub-object |
| `redirects` | The `redirects {}` call config sub-object |
| `security` | The `security {}` call config sub-object |
| `call` | The root call config object |

## Field definitions

Each field is declared as a key under the target with a table of properties:

```toml
[schema.scope_options]
silentOnRepeat = { type = "bool", default = "true" }
notification   = { type = "notification_expr" }
```

| Property | Required | Description |
|---|---|---|
| `type` | Yes | Type name -- either a built-in type or a name defined in `[types]` |
| `default` | No | Default value as a string. If absent, the field defaults to `null`. |
| `required` | No | Boolean. If `true`, the validator emits an error when the field is absent and the extension is active. Default `false`. |

## Type system

### Built-in types

| Name | Description |
|---|---|
| `string` | UTF-8 string |
| `int` | Integer |
| `float` | Floating point |
| `bool` | `true` or `false` |
| `null` | Null value |
| `any` | Any type |
| `array<T>` | Array of type T |
| `map<K, V>` | Object with key type K and value type V |
| `string?` | Nullable string (shorthand for `string | null`) |

### Custom types with tagged unions

Define custom types in the `[types]` section using `one_of` for tagged unions. A tagged union declares a set of variants -- the value must match exactly one.

```toml
[[types.notification_val.one_of]]
tag = "template"
[types.notification_val.one_of.fields]
name = "string"

[[types.notification_val.one_of]]
tag = "text"
[types.notification_val.one_of.fields]
value = "string"

[[types.notification_val.one_of]]
tag = "structured"
[types.notification_val.one_of.fields]
data = "any"
```

Each variant has a `tag` (the discriminator) and `fields` (the variant's data). In the rule language, you check the tag first, then access the variant's fields:

```
when $notif.tag eq "text"
let $message = $notif.value
```

You can also define simple type aliases:

```toml
[types.op_key_or_value]
type = "string"
```

## Example: laceNotifications schema

The `laceNotifications` extension registers `notification` and `silentOnRepeat` on scopes, conditions, and timeouts:

```toml
[schema.scope_options]
silentOnRepeat = { type = "bool", default = "true" }
notification   = { type = "notification_expr" }

[schema.condition_options]
silentOnRepeat = { type = "bool", default = "true" }
notification   = { type = "notification_expr" }

[schema.timeout]
notification = { type = "notification_expr" }
```

This means that when `laceNotifications` is active, `.expect()` and `.check()` scope `options {}` blocks accept a `notification` field and a `silentOnRepeat` field, and `timeout {}` blocks accept a `notification` field.

## TOML format constraint

TOML 1.0 forbids inline tables (`{ ... }`) from spanning multiple lines. For types with multiple fields, use sub-tables instead of multi-line inline tables:

```toml
# Wrong -- multi-line inline table
[types.event]
one_of = [
  { tag = "a", fields = { id = "string" } }
]

# Correct -- array of sub-tables
[[types.event.one_of]]
tag = "a"
[types.event.one_of.fields]
id = "string"
```

Simple single-line inline tables like `{ type = "bool", default = "true" }` are fine.
