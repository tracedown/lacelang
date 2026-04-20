# Contributing to Lace

Lace is an open-source project under the Apache 2.0 license. Contributions
are welcome from anyone — spec improvements, new executor implementations,
conformance vectors, tooling, and documentation.

## Conventions

- **Naming**: camelCase for identifiers, field names, and API surfaces.
  SCREAMING_SNAKE_CASE for error codes only.
- **Documentation**: every spec or behaviour change must update the
  relevant docs and wiki pages under `wiki/docs/`.
- **No purely stylistic changes**: refactoring whitespace, rewording
  comments, or reformatting code without a functional reason will not
  be accepted.

## How to contribute

### Proposing spec changes

The spec (`specs/lace-spec.md`, `specs/lace-extensions.md`) defines the
language. Changes to the spec affect every executor implementation, so
they require careful consideration.

1. **Open an issue or discussion** if you want feedback before
   implementing. This is optional — you can also go straight to a PR.
2. **Open a PR** with the proposed changes. Include:
   - The spec prose change
   - Updated EBNF grammar (`specs/lacelang.g4`) if syntax is affected
   - Updated JSON schemas (`specs/schemas/`) if wire formats change
   - Updated error codes (`specs/error-codes.json`) if new codes are
     needed
   - Updated checklists (`specs/checklist-core.md` or
     `specs/checklist-extensions.md`)
   - Updated wiki pages (`wiki/docs/`)
   - New or updated conformance vectors (`testkit/vectors/`)
3. Run `make verify` in `specs/` before submitting.
4. Discussion happens in PR comments. A maintainer must approve before
   merge.

### Adding conformance vectors

New test vectors exercise specific spec behaviours. To add one:

1. Pick the right `testkit/vectors/NN_*` directory.
2. Copy an existing vector as a template.
3. Run `npm run check-vectors` from `specs/tools/` to validate the
   format and error-code references.
4. Run `lace-conformance` against a reference executor to confirm the
   vector passes.

### Improving the grammar

If you change `specs/lacelang.g4`:

1. Add at least one sample to `specs/grammar-tests/positive/` or
   `specs/grammar-tests/negative/` demonstrating the change.
2. If the change affects the AST structure, update `specs/schemas/ast.json`
   and add a `parse` conformance vector.
3. Run `make verify` in `specs/`.

### Registering a new executor implementation

Lace is designed for independent executor implementations in any
language. To register yours:

1. Ensure your executor passes the conformance suite:
   ```bash
   lace-conformance -c "your-executor-command"
   ```
2. Open a PR adding a link to the "Known executor implementations"
   table in `README.md`. Include:
   - Repository URL
   - Language
   - Conformance level (full or partial, with omissions listed)
   - Spec version it conforms to (from `lace-executor.toml`
     `[executor].conforms_to`)
3. The maintainers will review your implementation, verify conformance,
   and approve.

See `specs/checklist-core.md` and `specs/checklist-extensions.md` for
the full list of behaviours your executor must support. See
`implementers/` in the wiki for a guided walkthrough.

### Documentation and wiki

The wiki lives in `wiki/docs/` and is built with mkdocs-material. When
changing spec behaviour, update the corresponding wiki page — the wiki
is the user-facing documentation, the spec is the implementer reference.

To preview locally:

```bash
cd wiki
pip install mkdocs-material
mkdocs serve
```

## Version

The current spec version is defined in the `VERSION` file at the
repository root. Spec, grammar, schemas, error codes, conformance
vectors, and the testkit all share this version. Executor
implementations declare which spec version they conform to via
`lace-executor.toml`.

The version is bumped **only** when the specification changes in a way
that affects executor behaviour or wire formats — new syntax, new
result fields, changed semantics, new error codes, etc. Refactoring
prose, fixing typos, reorganizing documentation, adding wiki pages,
or updating tooling does **not** warrant a version bump. If an executor
built against the previous version would still pass the conformance
suite unchanged, the version stays the same.

## Review process

All contributions require approval from a project maintainer before
merge. Discussion happens in PR comments — no separate approval process
is needed.

## License

By contributing, you agree that your contributions will be licensed
under the Apache License 2.0, the same license as the project.
