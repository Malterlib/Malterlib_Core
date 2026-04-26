#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Pre-commit hook for Malterlib binary repositories.
#
# Verifies that every staged file's actual content type matches its LFS state:
#   - Files whose actual content is binary MUST be staged as LFS pointers.
#   - Files whose actual content is text MUST be staged as raw content.
#
# This catches two kinds of drift between an upstream binary release and the
# .gitattributes whitelist:
#   1. A path that was text in the old version became binary in the new one,
#      but .gitattributes still excludes its extension/path from LFS, so the
#      raw binary would be committed straight into Git.
#   2. A new text file was added under a path that defaults to LFS (no
#      whitelist match), so a small text file would be needlessly stored as
#      an LFS object.
#
# Both the LFS state and the actual content type are read from the staged
# blob, never from the working tree. This matters for partial commits where
# the index and the working tree disagree (e.g. user staged content X, then
# edited the working tree to a different content type) — checking the
# working tree there would let bugs slip through or fire false positives.

set -u

LfsPointerSignature="version https://git-lfs.github.com/spec/v1"
SignatureLen=${#LfsPointerSignature}

Errors=0
Warnings=0

# Locate the LFS object store(s). `git lfs env` reports the actual paths
# in use, which may differ from the default `.git/lfs/objects/` (this
# project, for example, uses a shared store at `/opt/Source/LFSStorage/`).
# `LocalReferenceDirs` is a comma-separated list of alternate stores that
# LFS will also consult.
LfsEnv=$(git lfs env 2>/dev/null)
LfsMediaDir=$(printf '%s\n' "$LfsEnv" | sed -n 's/^LocalMediaDir=//p')
LfsReferenceDirs=$(printf '%s\n' "$LfsEnv" | sed -n 's/^LocalReferenceDirs=//p' | tr ',' ' ')

# Reusable temp file for materializing raw blobs. Plain `mktemp` (no `-t`)
# is portable across BSD (macOS) and GNU (Linux) — their `-t` template
# semantics are incompatible. Fail closed if mktemp fails: an unwritable
# TMPDIR would otherwise let the hook silently pass without checking
# anything.
BlobTmp=$(mktemp) || {
	echo "ERROR: pre-commit-lfs-check: failed to create temp file." >&2
	exit 2
}
trap 'rm -f "$BlobTmp"' EXIT

fg_PathIsBinary() {
	# Empty files are not binary. `file --mime-encoding` reports zero-byte
	# input as "binary", which would falsely reject placeholders like .keep
	# committed as raw, and falsely accept zero-byte LFS objects.
	[ -s "$1" ] || return 1
	[ "$(file --mime-encoding -b -- "$1" 2>/dev/null)" = "binary" ]
}

fg_PathIsLfsPointer() {
	# Compare the first SignatureLen bytes of the file $1 to the LFS pointer
	# signature without ever capturing potentially-binary bytes into a shell
	# variable (which would trigger bash's "ignored null byte" warning).
	head -c "$SignatureLen" -- "$1" 2>/dev/null \
		| cmp -s - <(printf '%s' "$LfsPointerSignature")
}

fg_FindLfsObject() {
	# Search all configured LFS stores for the object with oid $1 and print
	# its path on stdout. Return non-zero if not found.
	local Oid="$1" Store Path
	for Store in "$LfsMediaDir" $LfsReferenceDirs; do
		[ -n "$Store" ] || continue
		Path="$Store/${Oid:0:2}/${Oid:2:2}/$Oid"
		if [ -f "$Path" ]; then
			printf '%s\n' "$Path"
			return 0
		fi
	done
	return 1
}

while IFS= read -r -d '' File; do
	read -r StagedMode BlobHash _ < <(git ls-files -s -- "$File")
	# Skip non-regular blobs (symlinks 120000, gitlinks 160000): Git's filter
	# drivers never run on those, so they cannot be in a wrong LFS state.
	if [ "$StagedMode" != "100644" ] && [ "$StagedMode" != "100755" ]; then
		continue
	fi
	if [ -z "${BlobHash:-}" ]; then
		continue
	fi

	# Materialize the staged blob to a temp file. Avoids piping binary
	# bytes through bash command substitution, which would warn about
	# embedded NUL bytes.
	git cat-file -p "$BlobHash" > "$BlobTmp" 2>/dev/null

	if fg_PathIsLfsPointer "$BlobTmp"; then
		# Staged as an LFS pointer — find the actual content in the LFS
		# object store and classify it directly with `file`.
		Oid=$(awk -F'sha256:' '/^oid sha256:/{print $2; exit}' "$BlobTmp")
		if [ -z "$Oid" ]; then
			echo "ERROR: $File: malformed LFS pointer (no sha256 oid)." >&2
			Errors=$((Errors + 1))
			continue
		fi

		ObjectPath=$(fg_FindLfsObject "$Oid") || {
			echo "WARN:  $File: LFS object not present locally; skipping content-type check. Run 'git lfs fetch' to enable verification." >&2
			Warnings=$((Warnings + 1))
			continue
		}

		if ! fg_PathIsBinary "$ObjectPath"; then
			echo "ERROR: $File: stored in LFS but actual content is text." >&2
			echo "       Add a matching exclusion to .gitattributes (e.g. an extension or path) and run 'git add --renormalize $File'." >&2
			Errors=$((Errors + 1))
		fi
	else
		# Staged as raw content — classify the blob directly.
		if fg_PathIsBinary "$BlobTmp"; then
			echo "ERROR: $File: stored as raw content but actual content is binary." >&2
			echo "       Update .gitattributes to route this path through LFS, then run 'git add --renormalize $File'." >&2
			Errors=$((Errors + 1))
		fi
	fi
# `T` covers staged type changes (e.g. symlink replaced by a regular file in
# a new upstream binary release) — the existing mode check inside the loop
# still skips symlinks/gitlinks.
done < <(git diff --cached --name-only --diff-filter=ACMRT -z)

if [ "$Errors" -gt 0 ]; then
	echo "" >&2
	echo "Pre-commit LFS check failed: $Errors file(s) have wrong LFS state for their content type." >&2
	exit 1
fi

if [ "$Warnings" -gt 0 ]; then
	echo "" >&2
	echo "Pre-commit LFS check completed with $Warnings warning(s) (LFS objects not available locally)." >&2
fi

exit 0
