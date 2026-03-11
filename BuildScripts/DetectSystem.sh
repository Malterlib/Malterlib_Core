#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

set -e

SysName=$(uname -s)
ProcessorArch=$(uname -m)

function CallDirect()
{
	echo Calling Direct: $1
	$0 "$@"
}

if [[ $SysName ==  MSYS* ]] || [[ $SysName ==  MINGW* ]] ; then
	function CallDirect()
	{
		local ProgramToCall=$1
		shift
		DirectMinGWCallParams="$@" cmd.exe //C "$ProgramToCall %DirectMinGWCallParams%"
	}
fi

function TempPath()
{
	if [ ! "$TEMP" == "" ] ; then
		echo "$TEMP"
	elif [ ! "$TMP" == "" ] ; then
		echo "$TMP"
	elif [ ! "$TMPDIR" == "" ] ; then
		echo "$TMPDIR"
	else
		echo "/tmp"
	fi
}

BuildSystemRoot=$(cd "${BASH_SOURCE%/*}/../../.." ; echo $PWD)
if [[ "$MLBuildBuildSystemRoot" != "" ]]; then
	BuildSystemRoot="$BuildSystemRoot/$MLBuildBuildSystemRoot"
fi

function ResolveBootstrapVersion()
{
	if [[ "${BootstrapVersion-}" == "" ]]; then
		local BootstrapVersionPath="$BuildSystemRoot/Malterlib/Core/Bootstrap.version"
		if [ -e "$BootstrapVersionPath" ]; then
			export BootstrapVersion=$(<"$BootstrapVersionPath")
		fi
	fi
}

if [[ "${MalterlibUseGlobalBinaries-}" == "true" ]] && [[ "${MalterlibBinariesDir-}" == "" ]]; then
	ResolveBootstrapVersion
	if [[ "${MalterlibBootstrapBinariesPath-}" == "" ]] && [[ "${BootstrapVersion-}" != "" ]]; then
		export MalterlibBootstrapBinariesPath="$HOME/.Malterlib/bootstrap/$BootstrapVersion"
	fi

	if [[ "${MalterlibBootstrapBinariesPath-}" != "" ]] && [ -d "$MalterlibBootstrapBinariesPath" ]; then
		export MalterlibBinariesDir="$MalterlibBootstrapBinariesPath"
	fi
fi

source "$BuildSystemRoot/Malterlib/Core/Scripts/Detect.sh"
export MToolPath="$MToolDirectory"

function RefreshToolPathVariables()
{
	export MToolDirectory="$MToolPath"
	export MToolExecutable="$MToolDirectory/${MToolExecutable##*/}"
	export MalterlibExecutable="$MToolDirectory/${MalterlibExecutable##*/}"
}

RefreshToolPathVariables

MToolExecutableName=MTool
if [[ "$MalterlibPlatform" ==  Windows ]]; then
	MToolExecutableName=MTool.exe
fi

if [[ "${MalterlibUseGlobalBinaries-}" == "true" ]] && ! [ -e "$MToolPath/$MToolExecutableName" ]; then
	"$BuildSystemRoot/mib" bootstrap_only
	ResolveBootstrapVersion
	if [[ "${MalterlibBootstrapBinariesPath-}" == "" ]] && [[ "${BootstrapVersion-}" != "" ]]; then
		export MalterlibBootstrapBinariesPath="$HOME/.Malterlib/bootstrap/$BootstrapVersion"
	fi
	if [[ "${MalterlibBootstrapBinariesPath-}" != "" ]] && [ -d "$MalterlibBootstrapBinariesPath" ]; then
		export MToolPath="$MalterlibBootstrapBinariesPath"
		RefreshToolPathVariables
	fi
fi

if [[ "$MalterlibPlatform" ==  Windows ]]; then
	if [[ "${MalterlibUseGlobalBinaries-}" != "true" ]] && [[ "$MToolPath" != "$BuildSystemRoot/BuildSystem/SafeMib/Binaries" ]]; then
		"$BuildSystemRoot/mib" bootstrap_only
		export MToolPath="$BuildSystemRoot/BuildSystem/SafeMib/Binaries"
		RefreshToolPathVariables
	fi

	function p4()
	{
		PWD= "$OriginalP4" "$@"
	}
	export p4
	function MTool()
	{
		MTool.exe "$@"
	}
	export MTool
fi

function AddToPathIfNotAdded()
{
	if [ -d "$1" ]; then
		if [[ ":$PATH:" != *":$1:"* ]]; then
			export PATH="$1:$PATH"
		fi
	fi
}

AddToPathIfNotAdded "$MToolPath"

export AddToPathIfNotAdded

export CallDirect

export BuildSystemRoot
if [ -e "$BuildSystemRoot" ]; then
	pushd "$BuildSystemRoot" > /dev/null
fi
