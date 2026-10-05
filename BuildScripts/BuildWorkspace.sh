#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: BuildWorkspace.sh [Options] Workspace Platform Architecture Configuration
# The options are in BuildOptions.sh

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"
source "$DIR/BuildOptions.sh"

BuildOptionsParse "$@" || exit 1
set -- "${BuildOptionsArguments[@]}"

source "$DIR/BuildSystemSettings.sh"

BuildOptionsGenerate "${1:-Tests}"

if [[ "$MalterlibGenerateImportCaches" == "true" ]]; then
	echo "The build system in '$MalterlibBuildSystemDir' generates import caches (MalterlibGenerateImportCaches), so it is not built." >&2
	echo "Remove the setting from its UserSettings.MSettings to build." >&2
	exit 1
fi

if [[ "$MalterlibGenerator" == "Ninja" ]] ; then
	"$DIR/BuildNinjaWorkspace.sh" "$@"
	exit $?
elif [[ "$MalterlibGenerator" =~ ^VisualStudio ]] ; then
	"$DIR/BuildVisualStudioWorkspace.sh" "$@"
	exit $?
elif [[ "$MalterlibGenerator" =~ ^Xcode ]] ; then
	"$DIR/BuildXcodeWorkspace.sh" "$@"
	exit $?
else
	echo "Unknown generator '$BuildGenerator', aborting build"
	exit 1
fi
