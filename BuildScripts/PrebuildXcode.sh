#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: PrebuildXcode.sh Buildsystem Workspace [Workspaces]

# EnvVar: MalterlibPreBuildNoClean

set -e

export MalterlibDoingProductBuild=true

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

BuildSystem=$1
Generator=
Extension=MBuildSystem

shift

if [[ "$MalterlibPlatform" == "OSX" ]] ; then
	# Set Xcode build location to legacy
	defaults write com.apple.dt.Xcode IDEBuildLocationStyle DeterminedByTargets
	# Set Xcode to use malterlib toolchain
	#defaults write com.apple.dt.xcode DVTDefaultToolchainOverrideIdentifer org.malterlib.1.0
fi

for Argument in "$@" ; do
	if [[ $Argument = Generator\=* ]] ; then
		Generator="${Argument/Generator\=/}"
		echo Set generator to: $Generator
	elif [[ $Argument = Extension\=* ]] ; then
		Extension="${Argument/Extension\=/}"
		echo Set extension to: $Extension
	else
		echo Generating $Argument
		which MTool
		set +e
		"$BuildSystemRoot/mib" generate --build-system "$BuildSystem.$Extension" --generator "$Generator" --no-use-user-settings  --action ReBuild "$Argument"
		GenError="$?"
		set -e
		if [[ $GenError -ne 0 ]] && [[ $GenError -ne 2 ]] ; then
			echo "Build system generation failed with $GenError, aborting"
			exit 1
		fi
	fi
done

source ./BuildSystem/SharedBuildSettings.sh

Generator=

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

