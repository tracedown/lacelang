# Packaging

Per-distribution recipes for `lacelang-testkit`.

Every target builds the harness with `EMBED_VECTORS=1` so the installed
binary is self-contained — no data-directory discovery at runtime.

## Debian / Ubuntu

```bash
cd lacelang/testkit
dpkg-buildpackage -us -uc -b         # builds ../lacelang-testkit_<ver>.deb
```

The `debian/` directory here is a source-tree alternative to Debian's
conventional `debian/` at the repo root — copy or symlink into a build
tree as needed.

### What install/remove does

- **Install**: `postinst` runs `/usr/share/lacelang-testkit/generate-certs.sh` to pre-populate `/usr/share/lacelang-testkit/certs` with the TLS scenario pack. Needs `openssl` on PATH. If missing, install fails — the harness requires TLS certs (auto-generates to a temp dir at runtime as a fallback, but the package should provide them).
- **Remove**: `prerm` wipes only the generated cert files (never the surrounding dir if the admin added their own).

## Homebrew

The formula targets a released tag. Update `url` and `sha256` per release.

```bash
brew tap lacelang/lacelang
brew install lacelang-testkit
```

## OpenWRT (opkg)

For embedded/gateway deployments where the Lace runner itself is a C
executor, the testkit can be shipped as an `.ipk`:

```bash
opkg-utils build packaging/opkg .    # outputs lacelang-testkit_0.9.1_all.ipk
```

## Release checklist

1. Bump `testkit/src/main.c` `LACE_CONFORMANCE_VERSION`.
2. Bump `packaging/debian/changelog` and `packaging/opkg/control` version.
3. Tag the release: `git tag testkit-vX.Y.Z`.
4. Update `packaging/homebrew/lacelang-testkit.rb` `url` + `sha256`.
5. Publish per target:
   - deb → apt repository / GitHub Releases
   - brew → push formula to tap repo
   - opkg → upload `.ipk` to feeds
