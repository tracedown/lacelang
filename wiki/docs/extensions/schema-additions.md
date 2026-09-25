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

### Where registered fields land

- `scope_options` and `condition_options` fields appear directly in the `options {}` object -- a rule reads `scope.options?.notification` or `condition.options?.notification`.
- Fields registered on `timeout`, `redirects`, `security` and `call` are parsed into an `extensions` sub-object of the owning object and surface there in the resolved call config (`calls[n].config` in the result, `call.config` in hook context). Read them as `call.config.timeout?.extensions?.notification`, or `call.config.extensions?.<field>` for root-level (`call`) fields.

For example, with `laceNotifications` active, this script:

```lace
get("https://api.example.com/login", {
  timeout: { ms: 500, notification: text("Login timed out") }
})
.expect(status: 200)
```

records the resolved timeout config as `{ "ms": 500, "extensions": { "notification": { "tag": "text", "value": "Login timed out" } }, "action": "fail", "retries": 0 }`.

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
| `default` | No | Documented default, as a string. Informational only: executors do **not** inject it into `options` or `config` objects. A rule sees `null` for an absent field and must apply the default itself (`laceNotifications`' `is_silent` function is the pattern). |
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

Define custom types in the `[types]` section using `one_of` for tagged unions. A tagged union declares a set of variants -- the value must match exactly one. This is how `laceNotifications` declares its two notification types:

```toml
# The resolved type that appears in the result.
[types.notification_val]
one_of = [
  { tag = "template",   fields = { name  = "string" } },
  { tag = "text",       fields = { value = "string" } },
  { tag = "structured", fields = { data  = "any" } }
]

# The scripting-time superset: adds op_map, resolved to a
# notification_val before emission.
[types.notification_expr]
one_of = [
  { tag = "template",   fields = { name  = "string" } },
  { tag = "text",       fields = { value = "string" } },
  { tag = "structured", fields = { data  = "any" } },
  { tag = "op_map",     fields = { ops   = "map<string, notification_expr>" } }
]
```

Each variant has a `tag` (the discriminator) and `fields` (the variant's data). In the rule language, you check the tag first, then access the variant's fields:

```
when $notif.tag eq "text"
let $message = $notif.value
```

**Tag constructors.** Every `one_of` variant is callable by its tag, both from `.lace` option values and from rule bodies: `text("...")`, `template("...")`, `structured({ ... })`, `op_map({ ... })`. The call takes the variant's fields as positional arguments and produces `{ tag: "<tag>", <field>: ... }` -- `text("down")` is `{ tag: "text", value: "down" }`.

You can also define simple type aliases:

```toml
[types.op_key_or_value]
type = "string"
```

## Example: laceNotifications schema

The `laceNotifications` extension registers `notification` and `silentOnRepeat` on scopes and conditions, and `notification` alone on timeouts:

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

This means that when `laceNotifications` is active, `.expect()` / `.check()` scope and `.assert()` condition `options {}` blocks accept `notification` and `silentOnRepeat`, and `timeout {}` blocks accept `notification` (recorded under `config.timeout.extensions.notification`). There is no `silentOnRepeat` on timeouts.

## TOML format constraint

`.laceext` files must parse with any TOML 1.0 parser, and TOML 1.0 forbids an inline table (`{ ... }`) from spanning multiple lines. A multi-line *array* whose elements are each a single-line inline table is valid -- that is the form the bundled extensions use for `one_of` lists:

```toml
# Wrong -- an inline table broken across lines
[types.event]
one_of = [
  { tag = "a",
    fields = { id = "string" } }
]

# Correct -- each inline table on one line (the array may span lines)
[types.event]
one_of = [
  { tag = "a", fields = { id = "string" } },
  { tag = "b", fields = { name = "string" } }
]

# Also correct -- array of sub-tables
[[types.event.one_of]]
tag = "a"
[types.event.one_of.fields]
id = "string"
```

Single-line inline tables like `{ type = "bool", default = "true" }` are always fine.
