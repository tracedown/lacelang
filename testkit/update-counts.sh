#!/usr/bin/env bash
# update-counts.sh — refresh per-dir COUNT files for every leaf vector dir.
#
# Each leaf vector directory (testkit/vectors/NN_*/ and extensions/*/vectors/)
# holds a COUNT file containing the integer number of `*.json` test vectors
# in that directory. The vector-count-filter.sh git filter reads these to
# render {{ vectorCount[:key] }} placeholders in markdown without having to
# walk the tree at every clean.
#
# Run after adding or removing vectors:
#   bash testkit/update-counts.sh
#
# Then `git add` the affected COUNT files.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

write_count() {
    local dir="$1"
    local count
    count=$(find "$dir" -maxdepth 1 -name '*.json' | wc -l)
    printf '%s\n' "$count" > "$dir/COUNT"
    printf '  %s -> %d\n' "${dir#"$ROOT/"}/COUNT" "$count"
}

echo "Core vector dirs:"
for dir in "$ROOT/testkit/vectors"/*/; do
    [ -d "$dir" ] || continue
    write_count "$dir"
done

echo "Extension vector dirs:"
while IFS= read -r dir; do
    write_count "$dir"
done < <(find "$ROOT/extensions" -type d -name vectors | sort)

echo "Done."
