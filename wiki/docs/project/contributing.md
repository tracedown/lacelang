# Contributing

How to contribute to the Lace language specification, grammar, schemas, vectors, and testkit.

## To the Spec Prose (`specs/*.md`)

Spec changes that affect executor behaviour or wire formats need to flow through to the `.g4`, JSON schemas, and error-code registry. Run `make verify` before opening a PR.

## To the Grammar (`specs/lacelang.g4`)

If you change the grammar, add at least one sample to `grammar-tests/positive/` or `grammar-tests/negative/` demonstrating the change. If the change affects the AST structure, update `schemas/ast.json` and add a `parse` conformance vector.

## To the Schemas (`specs/schemas/*.json`)

Schema changes are breaking changes for every executor. Bump the schema `$id` version and document the migration in a changelog entry.

## To the Test Vectors (`testkit/vectors/*.json`)

New vectors: pick the right `vectors/NN_*` directory, copy an existing vector as a template, and run `npm run check-vectors` from `specs/tools/` to validate the format.

## To the Testkit C Harness (`testkit/src/`)

See `testkit/README.md` for architecture and current status.

## Verifying Spec Artifacts

```bash
cd specs
make verify
```

This runs:

1. **Grammar** -- generates the parser from `lacelang.g4`, then parses every sample under `grammar-tests/{positive,negative}/`. Positives must parse cleanly; negatives must produce parse errors.
2. **Schemas** -- compiles every JSON schema under `schemas/` and verifies the error-code registry has no duplicates.
3. **Extensions** -- TOML-parses every `.laceext` under `../extensions/` and validates against the laceext schema.
4. **Vectors** -- validates every conformance vector under `../testkit/vectors/` against the vector schema and cross-checks referenced error codes against the registry.

Requires Java (JDK 11+) for the grammar tests and Node.js for the schema/vector checks. Both are downloaded/installed automatically into `build/` and `tools/node_modules/` respectively.
