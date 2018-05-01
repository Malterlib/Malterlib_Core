#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: Postbuild.sh Workspace [Workspaces]

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

if [[ "$MalterlibPlatform" == "Windows" ]] ; then
	"$DIR/PostbuildVisualStudio.sh" "$@"
	exit $?
elif [[ "$MalterlibPlatform" == "OSX" ]] || [[ "$MalterlibPlatform" == "Linux" ]] ; then
	"$DIR/PostbuildXcode.sh" "$@"
	exit $?
else
	echo Unknown system, aborting build
	exit 1
fi
