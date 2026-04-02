#!/bin/bash
# Copyright Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Skip when this checkout was driven by mib itself. MalterlibInvocationCommands
# is a space-separated stack of mib operation tags inherited into nested git
# invocations. Only operations that actually invoke "git checkout" (and would
# otherwise re-enter mib via this hook) are listed; the rest of the tags mib
# emits are diagnostic.
#   handle-repos  - sub-repo reconciliation in fp_HandleRepositories
#   branch-root   - root-repo branch switch in fp_BranchRootRepo
#   force-branch  - per-repo branch switch in fp_ForceBranchRepos
#   repo-commit   - path-restricted checkout when staging the merge-base state
#
# The "repo-commit" tag intentionally covers only path-restricted checkouts
# (`git checkout <hash> -- <paths>`, $3 == 0). repo-commit may temporarily
# rewrite tracked file contents but never moves HEAD, so mib must NOT
# reconcile on top — the outer flow (fg_RestoreRepositoryIndex) restores
# the original state, and a reconcile would fight that restoration.
#
# We intentionally do NOT gate this hook on the branch-checkout flag ($3 == 1).
# Bare `git checkout -- <paths>` outside mib does not move HEAD either, so a
# subsequent `mib update-repos` is a no-op on the workspace; running it keeps
# the hook contract simple and costs nothing meaningful.
for cmd in $MalterlibInvocationCommands; do
	case "$cmd" in
		handle-repos|branch-root|force-branch|repo-commit)
			exit 0
			;;
	esac
done

# Skip if a rebase, cherry-pick, or merge is in progress
GitDir="$(git rev-parse --git-dir)"
if [ -d "$GitDir/rebase-merge" ] || [ -d "$GitDir/rebase-apply" ] || [ -f "$GitDir/CHERRY_PICK_HEAD" ] || [ -f "$GitDir/MERGE_HEAD" ]; then
	exit 0
fi

# Skip if in detached HEAD state (e.g. interactive rebase, edit commit history)
if ! git symbolic-ref -q HEAD > /dev/null 2>&1; then
	exit 0
fi

# Run update-repos after branch switch
RepoRoot="$(git rev-parse --show-toplevel)"

# Unset git environment variables set by the hook caller
# to prevent them from leaking into mib's internal git operations
unset GIT_DIR GIT_WORK_TREE GIT_PREFIX GIT_EXEC_PATH

# Detect environments that don't support ANSI escape codes
MibArgs=()
if [[ "$SSH_ASKPASS" == *"Sublime Merge"* ]] || [[ "$SSH_ASKPASS" == *"ssh-askpass-sublime"* ]]; then
	MibArgs+=(--no-color)
elif ! [ -t 1 ]; then
	MibArgs+=(--no-color)
fi

if [ -x "$RepoRoot/mib" ]; then
	"$RepoRoot/mib" update-repos "${MibArgs[@]}"
fi
