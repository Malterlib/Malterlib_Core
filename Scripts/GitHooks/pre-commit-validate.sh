#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

# The managed dispatcher records the workspace containing the local MTool.
# Preserve Git's environment, especially GIT_INDEX_FILE for partial commits.
RepoRoot=$(git rev-parse --show-toplevel)
WorkspaceRoot=${MalterlibHookWorkspaceRoot:-$RepoRoot}
ToolScript="$WorkspaceRoot/Malterlib/Core/Scripts/MTool.sh"
if [ ! -f "$ToolScript" ]; then
	printf 'ERROR: Commit validation cannot find MTool launcher: %s\n' "$ToolScript" >&2
	exit 2
fi

exec bash "$ToolScript" Validate --staged --working-directory "$RepoRoot" --no-color
