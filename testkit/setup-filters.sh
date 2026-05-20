#!/usr/bin/env bash
# setup-filters.sh — one-shot per-clone setup for the spec-version git filter.
#
# Run once after cloning this repo:
#   bash testkit/setup-filters.sh
#
# After this, `{{ specVersion }}` placeholders in *.md files are auto-rendered
# to `<VERSION><!-- sv -->` when staging, and restored to placeholders on
# checkout — see testkit/vector-count-filter.sh for the mechanism and
# .gitattributes for the wiring.

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git config filter.vectorcount.clean  'bash testkit/vector-count-filter.sh clean'
git config filter.vectorcount.smudge 'bash testkit/vector-count-filter.sh smudge'

echo "Configured filter.vectorcount.{clean,smudge} for $(pwd)."
echo
echo "Re-materializing markdown files so the working tree picks up placeholders:"

git ls-files -z '*.md' | xargs -0 rm --
git checkout-index --force -z --stdin < <(git ls-files -z '*.md')
git add --renormalize . > /dev/null

echo "  done — git status should be clean."
