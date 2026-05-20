#!/usr/bin/env bash
# vector-count-filter.sh — git clean/smudge filter for {{ vectorCount }}
# and {{ specVersion }} placeholders in markdown files.
#
# Clean  (working tree → index): replaces placeholders with actual values
#   followed by invisible HTML comment markers.
# Smudge (index → working tree): restores the placeholders.
#
# Placeholders:
#   {{ specVersion }}                   — content of lacelang/VERSION
#   {{ vectorCount }}                   — grand total across all leaf vector dirs
#   {{ vectorCount:core }}              — sum of testkit/vectors/*/COUNT
#   {{ vectorCount:ext }}               — sum of extensions/*/vectors/COUNT
#   {{ vectorCount:<section> }}         — testkit/vectors/<section>/COUNT
#                                         (e.g. {{ vectorCount:01_parsing }})
#   {{ vectorCount:ext.<extension> }}   — extensions/*/<extension>/vectors/COUNT
#                                         (e.g. {{ vectorCount:ext.laceNotifications }})
#
# Per-key counts are read from per-dir COUNT files maintained by
# testkit/update-counts.sh — run that script when vectors are added or removed.
# Per-clone setup: `bash testkit/setup-filters.sh`. Wiring lives in
# .gitattributes (`*.md filter=vectorcount`).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
VERSION_FILE="$ROOT/VERSION"

read_version() {
    cat "$VERSION_FILE" | tr -d '[:space:]'
}

read_count_file() {
    local f="$1"
    if [ -f "$f" ]; then
        cat "$f" | tr -d '[:space:]'
    else
        echo 0
    fi
}

# Build the key->count map. CORE_KEYS[*]=section name, EXT_KEYS[*]=extension name.
declare -A COUNTS=()
CORE_TOTAL=0
EXT_TOTAL=0

for dir in "$ROOT/testkit/vectors"/*/; do
    [ -d "$dir" ] || continue
    key=$(basename "$dir")
    count=$(read_count_file "$dir/COUNT")
    COUNTS["$key"]=$count
    CORE_TOTAL=$(( CORE_TOTAL + count ))
done

while IFS= read -r dir; do
    ext_name=$(basename "$(dirname "$dir")")
    count=$(read_count_file "$dir/COUNT")
    COUNTS["ext.$ext_name"]=$count
    EXT_TOTAL=$(( EXT_TOTAL + count ))
done < <(find "$ROOT/extensions" -type d -name vectors)

COUNTS["core"]=$CORE_TOTAL
COUNTS["ext"]=$EXT_TOTAL
GRAND_TOTAL=$(( CORE_TOTAL + EXT_TOTAL ))

case "${1:-}" in
    clean)
        VERSION=$(read_version)
        # Compose a sed script: keyed substitutions first, then the bare
        # {{ vectorCount }} grand total, then {{ specVersion }}.
        SED=""
        for key in "${!COUNTS[@]}"; do
            count=${COUNTS["$key"]}
            # Escape `.` in the key for the LHS pattern.
            escaped_key=${key//./\\.}
            SED+="s|{{ vectorCount:${escaped_key} }}|${count}<!-- vc:${key} -->|g;"
        done
        SED+="s|{{ vectorCount }}|${GRAND_TOTAL}<!-- vc -->|g;"
        SED+="s|{{ specVersion }}|${VERSION}<!-- sv -->|g;"
        sed "$SED"
        ;;
    smudge)
        # Keyed marker first (more specific), then bare, then sv.
        sed -E '
            s|[0-9]+<!-- vc:([A-Za-z0-9._-]+) -->|{{ vectorCount:\1 }}|g
            s|[0-9]+<!-- vc -->|{{ vectorCount }}|g
            s|[0-9]+\.[0-9]+\.[0-9]+<!-- sv -->|{{ specVersion }}|g
        '
        ;;
    count)
        echo "$GRAND_TOTAL"
        ;;
    version)
        read_version
        ;;
    keys)
        # Debug helper: list all known {{ vectorCount:<key> }} keys and their counts.
        for key in $(printf '%s\n' "${!COUNTS[@]}" | sort); do
            printf '  %-32s %d\n' "$key" "${COUNTS["$key"]}"
        done
        ;;
    *)
        echo "Usage: $0 {clean|smudge|count|version|keys}" >&2
        exit 1
        ;;
esac
