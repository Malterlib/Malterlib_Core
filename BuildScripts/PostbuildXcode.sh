#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: PostbuildVisualStudio.sh Workspace [Workspaces]

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

for Argument in "$@" ; do
	echo Cleaning workspace: $Argument
	CleanPath=${MalterlibCompiledFilesSourceBase}/${Argument}
	echo CleanPath: ${CleanPath}
	if [ -d "$CleanPath/Intermediate" ] ; then 
		MTool DeleteDirectoryRecursive "$CleanPath/Intermediate"
	fi
	if [ -d "$CleanPath/Output" ] ; then
		MTool DeleteDirectoryRecursive "$CleanPath/Output"
	fi
done

exit 0
