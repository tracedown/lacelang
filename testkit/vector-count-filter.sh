#!/usr/bin/env bash
# vector-count-filter.sh — git clean/smudge filter for {{ vectorCount }}
# and {{ specVersion }} placeholders in markdown files.
#
# Clean  (working tree → index): replaces placeholders with actual values
#   followed by invisible HTML comment markers.
# Smudge (index → working tree): restores the placeholders.
#
# Setup (run once per clone):
#   git config filter.vectorcount.clean  'bash testkit/vector-count-filter.sh clean'
#   git config filter.vectorcount.smudge 'bash testkit/vector-count-filter.sh smudge'
#
# Then in .gitattributes:
#   *.md filter=vectorcount

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
VECTORS_DIR="$SCRIPT_DIR/vectors"
EXT_DIR="$ROOT/extensions"
VERSION_FILE="$ROOT/VERSION"

count_vectors() {
    local core ext
    core=$(find "$VECTORS_DIR" -name "*.json" 2>/dev/null | wc -l)
    ext=$(find "$EXT_DIR" -path "*/vectors/*.json" 2>/dev/null | wc -l)
    echo $(( core + ext ))
}

read_version() {
    cat "$VERSION_FILE" | tr -d '[:space:]'
}

case "${1:-}" in
    clean)
        COUNT=$(count_vectors)
        VERSION=$(read_version)
        sed "s/{{ vectorCount }}/${COUNT}<!-- vc -->/g; s/{{ specVersion }}/${VERSION}<!-- sv -->/g"
        ;;
    smudge)
        sed 's/[0-9]\{1,\}<!-- vc -->/{{ vectorCount }}/g; s/[0-9]\{1,\}\.[0-9]\{1,\}\.[0-9]\{1,\}<!-- sv -->/{{ specVersion }}/g'
        ;;
    count)
        count_vectors
        ;;
    version)
        read_version
        ;;
    *)
        echo "Usage: $0 {clean|smudge|count|version}" >&2
        exit 1
        ;;
esac
