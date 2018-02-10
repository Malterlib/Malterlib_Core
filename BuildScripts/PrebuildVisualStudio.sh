#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: PrebuildVisualStudio.sh Buildsystem Workspace [Workspaces]

set -e

export MalterlibDoingProductBuild=true

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"
source "$DIR/MSysFixup.sh"

echo "CurrentDir=$PWD"
echo "Path=$PATH"

BuildSystem=$1
Generator=VisualStudio2017
Extension=MBuildSystem

shift

for Argument in "$@" ; do
	if [[ $Argument = Generator\=* ]] ; then
		Generator="${Argument/Generator\=/}"
		echo Set generator to: $Generator
	elif [[ $Argument = Extension\=* ]] ; then
		Extension="${Argument/Extension\=/}"
		echo Set extension to: $Extension
	else
		echo Generating $Argument
		set +e
		MTool BuildSystemGen $BuildSystem.$Extension "Workspace=$Argument" Generator=$Generator DisableUserSettings=true
		GenError="$?"
		set -e
		if [[ $GenError -ne 0 ]] && [[ $GenError -ne 2 ]] ; then
			echo "Build system generation failed with $GenError, aborting"
			exit 1
		fi
	fi
done

source ./BuildSystem/SharedBuildSettings.sh

Generator=VisualStudio2017

if [ "$MalterlibPreBuildNoClean" != "true" ] ; then
	for Argument in "$@" ; do
		if [[ $Argument = Generator\=* ]] ; then
			Generator="${Argument/Generator\=/}"
		elif [[ $Argument = Extension\=* ]] ; then
			Extension="${Argument/Extension\=/}"
		else
			echo Cleaning workspace: $Argument
			CleanPath=${MalterlibCompiledFilesSourceBase}/${Argument}
			echo CleanPath: ${CleanPath}
			if [ -d "$CleanPath/Intermediate" ] ; then 
				MTool DeleteDirectoryRecursive "$CleanPath/Intermediate"
			fi
			if [ -d "$CleanPath/Output" ] ; then
				MTool DeleteDirectoryRecursive "$CleanPath/Output"
			fi
		fi
	done
fi

exit 0
