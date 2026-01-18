#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildWorkspace.sh Workspace Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

if [[ "$MalterlibDisableBuildSystemGeneration" != "true" ]]; then
	Workspace="${1:-Tests}"
	eval "$MalterlibBuildSystemGenerateCommand --no-signal-changed \"$Workspace\""
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
