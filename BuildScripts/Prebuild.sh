#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: PrebuildVisualStudio.sh Buildsystem Workspace [Workspaces]

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SysName=$(uname -s)
if [[ $SysName ==  Darwin* ]] ; then
	export PATH="/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"
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

if [[ "$MalterlibPlatform" == "Windows" ]] ; then
	"$DIR/PrebuildVisualStudio.sh" "$@"
	exit $?
elif [[ "$MalterlibPlatform" == "OSX" ]] || [[ "$MalterlibPlatform" == "Linux" ]]; then
	"$DIR/PrebuildXcode.sh" "$@"
	exit $?
else
	echo Unknown system, aborting build
	exit 1
fi
