#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

set -e

export IsWindows=false
export IsLinux=false
export IsOSX=false

SysName=$(uname -s)
ProcessorArch=$(uname -m)

function CallDirect()
{
	echo Calling Direct: $1
	$0 "$@"
}

if [[ $SysName ==  MINGW* ]] ; then
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

if [ -d "$BuildSystemRoot/BuildSystem/Binaries/General" ]; then
	ToolsRoot="$BuildSystemRoot/BuildSystem/Binaries/General"
elif [ -d "$BuildSystemRoot/Binaries/Malterlib/General" ]; then
	ToolsRoot="$BuildSystemRoot/Binaries/Malterlib/General"
else
	ToolsRoot="$BuildSystemRoot/Malterlib/Tools/Binaries/MTool"
fi

if [[ $SysName ==  MINGW* ]] || [[ $SysName ==  CYGWIN* ]] || [[ $SysName ==  windows* ]] ; then

	if [[ "$ToolsRoot" != "$BuildSystemRoot/BuildSystem/Binaries/General" ]]; then
		mkdir -p "$BuildSystemRoot/BuildSystem/MTool"
		cp -r "$ToolsRoot/Windows/"* "$BuildSystemRoot/BuildSystem/MTool/"
		MToolPath="$BuildSystemRoot/BuildSystem/MTool"
	else
		MToolPath="$ToolsRoot/Windows"
	fi

	export PATH="$MToolPath:$PATH"

	export IsWindows=true
	function p4()
	{
		PWD= "$OriginalP4" "$@"
	}
	export p4
	function MTool()
	{
		MTool.com "$@"
	}
	export MTool

elif [[ $SysName ==  Darwin* ]] ; then
	if [[ $ProcessorArch == i*86 ]] ; then
		MToolPath="$ToolsRoot/OSX/x86"
	elif [[ $ProcessorArch == x86_64 ]] ; then
		MToolPath="$ToolsRoot/OSX/x64"
	else
		echo $ProcessorArch is not a recognized architecture and no build of MTool is available for it
		exit 1
	fi
	export PATH="$MToolPath:$PATH"
	export IsOSX=true
elif [[ $SysName ==  Linux* ]] ; then
	if [[ $ProcessorArch == i*86 ]] ; then
		MToolPath="$ToolsRoot/Linux/x86"
	elif [[ $ProcessorArch == x86_64 ]] ; then
		MToolPath="$ToolsRoot/Linux/x64"
	else
		echo $ProcessorArch is not a recognized architecture and no build of MTool is available for it
		exit 1
	fi
	export PATH="$MToolPath:$PATH"
	export IsLinux=true
else
	echo "Couldn't detect system"
fi

export CallDirect

export BuildSystemRoot
if [ -e "$BuildSystemRoot" ]; then
	pushd "$BuildSystemRoot" > /dev/null
fi
