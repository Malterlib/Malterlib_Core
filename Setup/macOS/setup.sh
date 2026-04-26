#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cd "$( dirname "${BASH_SOURCE[0]}" )"

set -e

if [[ "$MalterlibCompiledFiles" != "" ]]; then
	DependenciesDirectory="$MalterlibCompiledFiles/Dependencies"
else
	DependenciesDirectory=
	if [ -d /opt/CompiledFiles ]; then
		DependenciesDirectory="/opt/CompiledFiles/Dependencies"
	elif [ -d /CompiledFiles ]; then
		DependenciesDirectory="/CompiledFiles/Dependencies"
	else
		DependenciesDirectory="$HOME/.CompiledFiles/Dependencies"
	fi
fi

DependenciesVersion=3
DependenciesFile="$DependenciesDirectory/MalterlibDependencies.ver"

UpdateDependencies()
{
	echo Updating dependencies

	if ! which brew > /dev/null ; then
		echo Installing brew
		sudo ls
		NONINTERACTIVE=1 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
	elif [[ "$RunningCI" == "true" ]] || [[ "$CI" == "true" ]]; then
		HOMEBREW_NO_AUTO_UPDATE=1 brew install --quiet git git-lfs zstd
		return
	else
		brew update
		brew upgrade
	fi

	brew install cmake go graphviz ninja git git-lfs zstd
}

function DoInstall()
{
	UpdateDependencies

	mkdir -p "$DependenciesDirectory"
	echo $DependenciesVersion > "$DependenciesFile"
}

function CheckSetup()
{
	if [ -f "$DependenciesFile" ]; then
		CurrentVersion=`cat "$DependenciesFile"`
		if (( $CurrentVersion >= $DependenciesVersion )); then
			return 0
		fi
	fi

	echo
	echo To install/update dependencies needed to build Malterlib on macOS, you need to run:
	echo
	echo ./mib setup
	echo
	exit 1
}

if [ "$1" != "" ] ; then
	"$@"
else
	DoInstall
	echo Successful
fi
