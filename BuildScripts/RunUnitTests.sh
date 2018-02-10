#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: RunTests.sh Workspace Platform Architecture Configuration 

echo "Running: RunTests.sh"

echo "---Sourcing MSysFixup"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/MSysFixup.sh"

if [ "$MalterlibAutoBuild" != "true" ] ; then

	source "$DIR/DetectSystem.sh"

	pushd "$DIR/../../.." > /dev/null

	source ./BuildSystem/SharedBuildSettings.sh

	TestPath=$(TempPath)/Tests

	echo TestPath=$TestPath

	MTool BuildServerGet "Source=Test/$1/$2/$3/$4/*" "DestinationDir=${TestPath}"
	CheckErrors

	pushd "${TestPath}"

	ToRun=

	for ExePath in *.MainTest ; do
		ToRun+="( \"${ExePath%.MainTest}\" --Tests )
	"
	done

	MTool Launch +EchoCommand +Delay +LimitConcurrency $ToRun

	if [[ $? -ne 0 ]] ; then
		echo \"\"
		echo ERROR: Unit tests failed
		exit 1
	fi

	exit 0
fi

if [ "$MalterlibAutoBuild" == "true" ] ; then
	echo "---Running for AutoBuild"

	export MalterlibAutoBuildMTool="MTool"

	SysName=$(uname -s)
	ProcessorArch=$(uname -m)

	if [[ $SysName ==  MINGW* ]] || [[ $SysName ==  CYGWIN* ]] || [[ $SysName ==  windows* ]] ; then
		export MalterlibAutoBuildMTool="$PWD/MTool/Windows/MTool.com"
	elif [[ $SysName ==  Darwin* ]] ; then
		if [[ $ProcessorArch == i*86 ]] ; then
			export MalterlibAutoBuildMTool="$PWD/MTool/OSX/x86/MTool"
		elif [[ $ProcessorArch == x86_64 ]] ; then
			export MalterlibAutoBuildMTool="$PWD/MTool/OSX/x64/MTool"
		else
			echo $ProcessorArch is not a recognized architecture and no build of MTool is available for it
			exit 1
		fi
	elif [[ $SysName ==  Linux* ]] ; then
		if [[ $ProcessorArch == i*86 ]] ; then
			export MalterlibAutoBuildMTool="$PWD/MTool/Linux/x86/MTool"
		elif [[ $ProcessorArch == x86_64 ]] ; then
			export MalterlibAutoBuildMTool="$PWD/MTool/Linux/x64/MTool"
		else
			echo $ProcessorArch is not a recognized architecture and no build of MTool is available for it
			exit 1
		fi
	else
		echo "Couldn't detect system"
		exit 1
	fi

	# The unit test path used here needs all spaces and paranthesis removed from the 
	# config name.
	SafeConfigName=${4//[ \(\)]/}
	#`echo $4 | sed 's/[ \(\)]//g'`
	TestPath=$MalterlibAutoBuildTestRoot/$1/$2/$3/$SafeConfigName

	echo TestPath=$TestPath

	pushd "${TestPath}"

	echo "---Building ToRun list"

	ToRun=

	if test -n "$(find . -maxdepth 1 -name '*.MainTest' -print -quit)"
	then
		for ExePath in *.MainTest ; do
			ToRun+="( ${ExePath%.MainTest} --Tests )
		"
		done

		echo "---ToRunList: $ToRun"
	
		echo "---Launching Tests"

		$MalterlibAutoBuildMTool Launch +Time +EchoCommand +Delay +LimitConcurrency $ToRun

		if [[ $? -ne 0 ]] ; then
			echo \"\"
			echo ERROR: Unit tests failed
			exit 1
		fi
	else
		# No tests found.
		echo "---No tests found"
	fi


	exit 0
fi
