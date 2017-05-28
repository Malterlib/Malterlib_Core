#!/bin/bash

trap 'rc=$?; echo "${BASH_SOURCE}:${LINENO}: error: Trapped error: $rc"; exit $rc' ERR

set -e

ScriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

ClangVersion="$1"
OutputDirectory="$2"
MalterlibMainMalterlibRepo="$3"

unset MACOSX_DEPLOYMENT_TARGET
unset SDKROOT
unset PRODUCT_SPECIFIC_LDFLAGS
unset OTHER_CFLAGS_ONLY

if [ ! -e "$OutputDirectory" ]; then
	mkdir -p "$OutputDirectory"
	git clone -b $ClangVersion --recursive https://github.com/Malterlib/llvm-malterlib.git "$OutputDirectory"
fi

pushd "$OutputDirectory"

VersionTimeFile="$ScriptDir/llvm.$ClangVersion.versiontime"

ExpectedVersionTime=""
if [ -e "$VersionTimeFile" ]; then
	ExpectedVersionTime=`cat "$VersionTimeFile"`
fi

VersionTime=`git for-each-ref --format='%(committerdate:unix)' refs/heads/$ClangVersion`

if [[ $ExpectedVersionTime != "" ]] && [[ $VersionTime < $ExpectedVersionTime ]]; then
	echo Fetching new llvm version
	git fetch origin $ClangVersion
	git reset --hard origin/$ClangVersion
	git pull
	VersionTime=`git for-each-ref --format='%(committerdate:unix)' refs/heads/$ClangVersion`
fi

if [[ "$ExpectedVersionTime" == "" ]] || [[ $VersionTime > $ExpectedVersionTime ]]; then
	if [[ "$MalterlibMainMalterlibRepo" == "true" ]]; then
		echo $VersionTime > "$VersionTimeFile"
	fi
fi

BuildTimeFile="$OutputDirectory/build/buildversiontime"

BuildTime=""
if [ -e "$BuildTimeFile" ]; then
	BuildTime=`cat "$BuildTimeFile"`
fi

if [[ "$BuildTime" == "$VersionTime" ]]; then
	exit 0
fi

pushd Scripts
./build.sh

echo $VersionTime > "$BuildTimeFile"
