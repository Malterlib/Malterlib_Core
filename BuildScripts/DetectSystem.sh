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
	# Breaks cmd.exe bare-command resolution (vcvars/msbuild) on hardened machines
	unset NoDefaultCurrentDirectoryInExePath
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

source "$BuildSystemRoot/Malterlib/Core/Scripts/Detect.sh"
export MToolPath="$MToolDirectory"

if [[ "$MalterlibPlatform" ==  Windows ]]; then
	if [[ "$MToolPath" != "$BuildSystemRoot/BuildSystem/SafeMib/Binaries" ]]; then
		"$BuildSystemRoot/mib" bootstrap_only
		export MToolPath="$BuildSystemRoot/BuildSystem/SafeMib/Binaries"
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

export PATH="$MToolPath:$PATH"

export CallDirect

export BuildSystemRoot
if [ -e "$BuildSystemRoot" ]; then
	pushd "$BuildSystemRoot" > /dev/null
fi
