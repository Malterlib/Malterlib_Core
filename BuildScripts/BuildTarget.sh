#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: ./mib build_target Workspace Target Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

if [[ "$MalterlibPlatform" == "Windows" ]] ; then
	"$DIR/BuildVisualStudioTarget.sh" "$@"
	exit $?
elif [[ "$MalterlibPlatform" == "macOS" ]] ; then
	"$DIR/BuildXcodeTarget.sh" "$@"
	exit $?
else
	echo Unknown system, aborting build
	exit 1
fi

