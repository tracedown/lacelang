#!/usr/bin/env bash
# bump-version.sh — bump the spec version in VERSION and all conformance vectors.
#
# Usage:
#   bash testkit/bump-version.sh          # patch bump (0.9.1 -> 0.9.2)
#   bash testkit/bump-version.sh 1.0.0    # explicit new version

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
VERSION_FILE="$ROOT/VERSION"
VECTORS_DIR="$SCRIPT_DIR/vectors"

old_version=$(cat "$VERSION_FILE" | tr -d '[:space:]')

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    echo "Usage: bash testkit/bump-version.sh [new-version]"
    echo "  No args: patch bump (e.g. 0.9.1 -> 0.9.2)"
    echo "  With arg: set explicit version (e.g. 1.0.0)"
    exit 0
fi

if [[ -n "${1:-}" ]]; then
    new_version="$1"
    if ! echo "$new_version" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
        echo "Invalid version \"$new_version\" — must be semver (X.Y.Z)" >&2
        exit 1
    fi
else
    IFS='.' read -r major minor patch <<< "$old_version"
    if [[ -z "$major" || -z "$minor" || -z "$patch" ]]; then
        echo "Cannot parse VERSION \"$old_version\" as semver" >&2
        exit 1
    fi
    new_version="$major.$minor.$((patch + 1))"
fi

if [[ "$new_version" == "$old_version" ]]; then
    echo "New version \"$new_version\" is the same as current" >&2
    exit 1
fi

echo "$old_version -> $new_version"

# 1. Update VERSION file
printf '%s\n' "$new_version" > "$VERSION_FILE"
echo "  VERSION file updated"

# 2. Update all conformance vectors that contain the old version
updated=0
skipped=0

while IFS= read -r -d '' file; do
    if grep -q "\"$old_version\"" "$file"; then
        sed -i "s/\"version\": \"$old_version\"/\"version\": \"$new_version\"/g" "$file"
        updated=$((updated + 1))
    else
        skipped=$((skipped + 1))
    fi
done < <(find "$VECTORS_DIR" -name "*.json" -print0)

echo "  Vectors: $updated updated, $skipped unchanged"
