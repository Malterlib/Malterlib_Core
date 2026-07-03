#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Audits every tracked file in a Git repository for content-vs-LFS-state
# mismatches. The actual content type comes from the working-tree file
# (smudged content), and the LFS membership comes from `git lfs ls-files`.
#
# Two kinds of error:
#   - A file in LFS whose actual content is text.
#   - A file outside LFS whose actual content is binary.
#
# Usage: check-lfs-state.sh [<repo-path>]
#
# Exit status:
#   0  no errors (warnings allowed)
#   1  one or more files have wrong LFS state for their content type
#   2  invocation error (not a git repository, etc.)

set -u

if [ "$#" -gt 0 ]; then
	cd "$1" || exit 2
fi

# Normalize to the repo root so `git lfs ls-files` (always repo-root-relative)
# and `git ls-files --stage` (cwd-relative) agree on path scope and produce
# comparable lists. Without this, the audit gives wrong results when run
# from a subdirectory.
RepoTop=$(git rev-parse --show-toplevel 2>/dev/null) || {
	echo "ERROR: not inside a git repository." >&2
	exit 2
}
cd "$RepoTop" || exit 2

LfsPointerSignature="version https://git-lfs.github.com/spec/v1"
SignatureLen=${#LfsPointerSignature}

Errors=0
Warnings=0

fg_PathIsBinary() {
	# Empty files are not binary. `file --mime-encoding` reports zero-byte
	# input as "binary", which would falsely reject placeholders like .keep
	# committed as raw, and falsely accept zero-byte LFS objects.
	[ -s "$1" ] || return 1
	[ "$(file --mime-encoding -b -- "$1" 2>/dev/null)" = "binary" ]
}

fg_PathIsUnsmudgedPointer() {
	# Returns 0 if the file's first SignatureLen bytes match the LFS pointer
	# signature, meaning the smudge filter never ran. Use `cmp` to avoid
	# capturing potentially-binary bytes through bash command substitution
	# (which warns on embedded NULs in newer bash versions).
	head -c "$SignatureLen" -- "$1" 2>/dev/null \
		| cmp -s - <(printf '%s' "$LfsPointerSignature")
}

fg_StagedBlobIsLfsPointer() {
	# `git lfs ls-files` deduplicates by object: when several paths share
	# one blob, only the first path is listed and the rest land in the
	# non-LFS list even though they are stored as LFS pointers. Detect
	# that by reading the staged blob directly.
	git cat-file blob ":0:$1" 2>/dev/null \
		| head -c "$SignatureLen" \
		| cmp -s - <(printf '%s' "$LfsPointerSignature")
}

# --- Files tracked in LFS: must be binary ----------------------------------
# Plain `mktemp` (no `-t`) is portable across BSD (macOS) and GNU (Linux).
# BSD `mktemp -t prefix` and GNU `mktemp -t template` use incompatible
# template semantics — avoid both. Fail closed if mktemp can't allocate.
LfsList=$(mktemp) && AllList=$(mktemp) && NonLfsList=$(mktemp) || {
	echo "ERROR: check-lfs-state: failed to create temp file." >&2
	exit 2
}
trap 'rm -f "$LfsList" "$AllList" "$NonLfsList" "$NonLfsList.lfs-sorted"' EXIT
if ! git lfs ls-files --name-only > "$LfsList"; then
	echo "ERROR: check-lfs-state: failed to list Git LFS files." >&2
	exit 2
fi
LfsCount=$(wc -l < "$LfsList" | tr -d ' ')

while IFS= read -r File; do
	[ -f "$File" ] || continue

	if fg_PathIsUnsmudgedPointer "$File"; then
		echo "WARN:  $File: working tree contains an unsmudged LFS pointer; skipping content-type check. Run 'git lfs pull' to enable verification." >&2
		Warnings=$((Warnings + 1))
		continue
	fi

	if ! fg_PathIsBinary "$File"; then
		echo "ERROR: $File: stored in LFS but actual content is text." >&2
		echo "       Add a matching exclusion to .gitattributes (e.g. an extension or path) and run 'git add --renormalize $File'." >&2
		Errors=$((Errors + 1))
	fi
done < "$LfsList"

# --- Files NOT tracked in LFS: must be text --------------------------------
# Restrict to regular blobs; symlinks (120000) and gitlinks (160000) cannot
# be in a wrong LFS state because Git's filter drivers never run on them.

git ls-files --stage | awk '$1 == "100644" || $1 == "100755" {
	# Reconstruct the path: drop the "<mode> <hash> <stage>\t" prefix.
	$1=""; $2=""; $3="";
	sub(/^[ \t]+/, "");
	print
}' | sort > "$AllList"
sort "$LfsList" > "$NonLfsList.lfs-sorted"
comm -23 "$AllList" "$NonLfsList.lfs-sorted" > "$NonLfsList"
rm -f "$NonLfsList.lfs-sorted"
NonLfsCount=$(wc -l < "$NonLfsList" | tr -d ' ')

while IFS= read -r File; do
	[ -f "$File" ] || continue

	if fg_PathIsBinary "$File"; then
		if fg_StagedBlobIsLfsPointer "$File"; then
			# Stored in LFS after all — `git lfs ls-files` omitted this
			# path because another path shares the same blob.
			continue
		fi

		echo "ERROR: $File: stored as raw content but actual content is binary." >&2
		echo "       Update .gitattributes to route this path through LFS, then run 'git add --renormalize $File'." >&2
		Errors=$((Errors + 1))
	fi
done < "$NonLfsList"

echo ""
echo "Checked $LfsCount LFS file(s) and $NonLfsCount raw file(s) in $RepoTop."
if [ "$Errors" -gt 0 ]; then
	echo "$Errors error(s)."
fi
if [ "$Warnings" -gt 0 ]; then
	echo "$Warnings warning(s) (working tree contains unsmudged LFS pointers)."
fi
if [ "$Errors" -eq 0 ] && [ "$Warnings" -eq 0 ]; then
	echo "All files have correct LFS state."
fi

if [ "$Errors" -gt 0 ]; then
	exit 1
fi

exit 0
