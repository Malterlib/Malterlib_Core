#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: PrebuildVisualStudio.sh Buildsystem Workspace [Workspaces]

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SysName=$(uname -s)

function AddToPathIfNotAdded()
{
	if [ -d "$1" ]; then
		if [[ ":$PATH:" != *":$1:"* ]]; then
			export PATH="$1:$PATH"
		fi
	fi
}

if [[ $SysName ==  Darwin* ]] ; then
	AddToPathIfNotAdded "/usr/local/bin"
	AddToPathIfNotAdded "/usr/local/sbin"
	AddToPathIfNotAdded "/opt/homebrew/bin"
	AddToPathIfNotAdded "/opt/homebrew/sbin"
fi

pushd "$DIR/../../.." > /dev/null

if [[ "$MLBuildGit" != "" && "$MLBuildUseGit" == "1" ]]; then
	GitFolders=(${MLBuildGit//;/ })
	for GitFolder in "${GitFolders[@]}"; do
		Parts=(${GitFolder//=/ })
		Folder=${Parts[0]}
		Repo=${Parts[1]}
		if [[ ! -d "$Folder" ]]; then
			echo "Initializing: $Folder from $Repo"
			mkdir -p $Folder
			pushd $Folder > /dev/null
			git init .
			popd > /dev/null
		fi
		pushd $Folder > /dev/null
		echo "Fetching in $Folder from $Repo"
		export GIT_TERMINAL_PROMPT=0
		git config credential.helper store
		if ! git remote add origin "${Repo}" 2> /dev/null; then
			git remote set-url origin "${Repo}"
		fi
		git fetch
		popd > /dev/null
	done
	GitBranches=(${MLBuildBranch//;/ })
	for GitBranch in "${GitBranches[@]}"; do
		Parts=(${GitBranch//=/ })
		Folder=${Parts[0]}
		Branch=${Parts[1]}
		pushd $Folder > /dev/null
		echo "Checking out $Branch in $Folder"

		if [[ `git branch -r --list "origin/$Branch" --format="%(refname:lstrip=-2)"` != "origin/$Branch" ]]; then
			echo "Branch does not exist on remote: $Branch"
		fi

		git checkout -f -B "$Branch" "origin/$Branch"
		git clean -fd
		if [ -x mib ]; then
			./mib update_repos
		fi
		popd > /dev/null
	done
else
	./mib update_repos
fi

source "$DIR/DetectSystem.sh"

if [ -x "$MLBuildPrebuild" ]; then
	"./$MLBuildPrebuild" "$@"
fi

export MalterlibDoingProductBuild=true

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

BuildSystem="$1"
Generator="$Malterlib_Generator"
Extension=MBuildSystem

if ! [[ $# -gt 0 ]]; then
	echo "You have to specify the build system"
	exit 1
fi

shift

if ! [[ $# -gt 0 ]]; then
	echo "You have to specify the workspaces"
	exit 1
fi

GenerateAction="ReBuild"
if [ "$MalterlibPreBuildNoReBuild" == "true" ]; then
	GenerateAction="Build"
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

		if [[ "$Generator" =~ ^Xcode ]] && [[ "$MalterlibPlatform" == "macOS" ]] ; then
			# Set Xcode build location to legacy
			defaults write com.apple.dt.Xcode IDEBuildLocationStyle DeterminedByTargets
		fi

		set +e
		"$BuildSystemRoot/mib" generate --build-system "$BuildSystem.$Extension" --generator "$Generator" --no-use-user-settings  --action $GenerateAction "$Argument"
		GenError="$?"
		set -e
		if [[ $GenError -ne 0 ]] && [[ $GenError -ne 2 ]] ; then
			echo "Build system generation failed with $GenError, aborting"
			exit 1
		fi
	fi
done

source ./BuildSystem/SharedBuildSettings.sh

Generator="$Malterlib_Generator"
Extension=MBuildSystem

if [ "$MalterlibPreBuildNoClean" != "true" ] ; then
	for Argument in "$@" ; do
		if [[ $Argument = Generator\=* ]] ; then
			Generator="${Argument/Generator\=/}"
		elif [[ $Argument = Extension\=* ]] ; then
			Extension="${Argument/Extension\=/}"
		else
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
		fi
	done
fi

exit 0

