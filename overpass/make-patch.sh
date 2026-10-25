#!/bin/bash
# Turns what's changed in dxmt/ (on top of the pinned commit plus the existing patches) into the next numbered
# patch in overpass/patches. The working tree is left as it is.
#   overpass/make-patch.sh "subject" ["body"]
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)"
PATCHES="$SRC/overpass/patches"
SUBJECT="${1:?usage: make-patch.sh \"subject\" [\"body\"]}"
BODY="${2:-}"
cd "$SRC/dxmt"

PIN="$(git -C "$SRC" ls-tree HEAD dxmt | awk '{print $3}')"
IDENTITY=(-c user.name="$(git -C "$SRC" config user.name)" -c user.email="$(git -C "$SRC" config user.email)")
NEXT=$(( $(ls "$PATCHES"/*.patch 2>/dev/null | wc -l) + 1 ))
TMP_INDEX="$(mktemp)"
trap 'rm -f "$TMP_INDEX"' EXIT

# The tree as it is now (untracked files included), without touching the real index.
cp "$(git rev-parse --git-path index)" "$TMP_INDEX"
FULL=$(GIT_INDEX_FILE="$TMP_INDEX" sh -c 'git add -A && git write-tree')

# The tree the existing patches produce.
GIT_INDEX_FILE="$TMP_INDEX" git read-tree "$PIN"
if ls "$PATCHES"/*.patch >/dev/null 2>&1; then
    GIT_INDEX_FILE="$TMP_INDEX" git apply --cached "$PATCHES"/*.patch
fi
BASE=$(GIT_INDEX_FILE="$TMP_INDEX" git write-tree)

if [ "$BASE" = "$FULL" ]; then
    echo "Nothing new in dxmt/." >&2
    exit 1
fi

BASE_COMMIT=$(git "${IDENTITY[@]}" commit-tree "$BASE" -p "$PIN" -m base)
NEW_COMMIT=$(printf '%s\n\n%s\n' "$SUBJECT" "$BODY" | git "${IDENTITY[@]}" commit-tree "$FULL" -p "$BASE_COMMIT" -F -)
git format-patch -q -1 --start-number "$NEXT" -o "$PATCHES" --zero-commit "$NEW_COMMIT"
git diff --stat "$BASE_COMMIT" "$NEW_COMMIT" | tail -1
ls "$PATCHES" | tail -1
