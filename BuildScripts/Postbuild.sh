#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: Postbuild.sh Workspace [Workspaces]

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
