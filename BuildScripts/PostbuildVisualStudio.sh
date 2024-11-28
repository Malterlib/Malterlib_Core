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
	if [ -f "BuildSystem/Default/Files/$Argument/Paths.sh" ]; then
		source "BuildSystem/Default/Files/$Argument/Paths.sh"
		WorkspacePathVariableName="WorkspaceBase_$Argument"
		CleanPath=${!WorkspacePathVariableName}
	else
		CleanPath=${MalterlibCompiledFilesSourceBase}/${Argument}
	fi
	echo CleanPath: ${CleanPath}
	if [ -d "$CleanPath/Int" ] ; then
		MTool DeleteDirectoryRecursive "$CleanPath/Int"
	fi
	if [ -d "$CleanPath/Out" ] ; then
		MTool DeleteDirectoryRecursive "$CleanPath/Out"
	fi
done

exit 0
