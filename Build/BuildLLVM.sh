#!/bin/bash

trap 'rc=$?; echo "${BASH_SOURCE}:${LINENO}: error: Trapped error: $rc"; exit $rc' ERR

set -e

ScriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function LockFile
{
	if [ "$#" -ne 1 ]; then
		echo 'usage: LockFile [LOCKFILENAME]' 1>&2
		return 2
	fi
	LOCKFILE="$1"

	echo "$$" >"$LOCKFILE.$$"
	if ! ln "$LOCKFILE.$$" "$LOCKFILE" 2>/dev/null; then
		PID=`head -1 "$LOCKFILE"`
		if [ -z "$PID" ]; then
		   rm -f "$LOCKFILE"
		else
		   kill -0 "$PID" 2>/dev/null || rm -f "$LOCKFILE"
		fi

		if ! ln "$LOCKFILE.$$" "$LOCKFILE" 2>/dev/null; then
		   rm -f "$LOCKFILE.$$"
		   return 1
		fi
	fi

	rm -f "$LOCKFILE.$$"
	trap 'rm -f "$LOCKFILE"' EXIT

	return 0
}


ClangVersion="$1"
OutputDirectory="$2"
MalterlibMainMalterlibRepo="$3"

LockDir="$(dirname "$OutputDirectory")"
mkdir -p "$LockDir"

SECONDS=0
LastSeconds=-1
while ! LockFile "${OutputDirectory}.lock"; do
	ThisSeconds=$SECONDS
	if [[ "$ThisSeconds" != "$LastSeconds" ]] && [[ "$(($ThisSeconds % 10))" == "0" ]]; then
		echo Waiting for other llvm build to finish: $ThisSeconds s
	else
		sleep 1
	fi
	LastSeconds=$ThisSeconds
done

unset TOOLCHAINS
export PATH="/usr/local/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
unset MACOSX_DEPLOYMENT_TARGET
unset SDKROOT
unset PRODUCT_SPECIFIC_LDFLAGS
unset OTHER_CFLAGS_ONLY
unset CC
unset CLANG
unset CPLUSPLUS
unset LD
unset LDPLUSPLUS

function BuildClangVersion()
{
	Version=$1
	Suffx=$2
	BranchName="${Version}${Suffx}"

	if [ ! -e "$OutputDirectory$Suffx" ]; then
		mkdir -p "$OutputDirectory$Suffx"
		git clone -b $BranchName https://github.com/Malterlib/llvm-malterlib.git "$OutputDirectory$Suffx"
	fi

	pushd "$OutputDirectory$Suffx" > /dev/null

	VersionTimeFile="$ScriptDir/llvm.$BranchName.versiontime"

	ExpectedVersionTime=""
	if [ -e "$VersionTimeFile" ]; then
		ExpectedVersionTime=`cat "$VersionTimeFile"`
	fi

	VersionTime=`git for-each-ref --format='%(committerdate:unix)' refs/heads/$BranchName`

	if [[ $ExpectedVersionTime != "" ]] && [[ $VersionTime < $ExpectedVersionTime ]]; then
		echo Fetching new llvm version
		git fetch origin $BranchName
		git reset --hard origin/$BranchName
		git pull
		VersionTime=`git for-each-ref --format='%(committerdate:unix)' refs/heads/$BranchName`
	fi

	if [[ "$ExpectedVersionTime" == "" ]] || [[ $VersionTime > $ExpectedVersionTime ]]; then
		if [[ "$MalterlibMainMalterlibRepo" == "true" ]]; then
			echo $VersionTime > "$VersionTimeFile"
		fi
	fi

	BuildTimeFile="$OutputDirectory$Suffx/build/buildversiontime"

	BuildTime=""
	if [ -e "$BuildTimeFile" ]; then
		BuildTime=`cat "$BuildTimeFile"`
	fi

	if [[ "$BuildTime" == "$VersionTime" ]]; then
		return 0
	fi

	export MalterlibRepositoryHardReset=true

	./mib update_repos

	export TOOLCHAIN_DIR="$DT_TOOLCHAIN_DIR"

	pushd Scripts
	./build.sh
	popd

	popd > /dev/null

	echo $VersionTime > "$BuildTimeFile"
}

function UpdateToolChain()
{
	if [[ "$MalterlibUseCustomXcodeToolchain" != "true" ]]; then
		return
	fi

	BuildClangVersion $ClangVersion -xcode

	IntegerClangVersion=${ClangVersion/./}

	pushd "${OutputDirectory}-xcode" > /dev/null

	ToolchainVersionTime=`cat build/buildversiontime`

	ToolchainVersionFile="$HOME/Library/Developer/Toolchains/Malterlib.xctoolchain.xcode_version"
	ToolchainLLVMVersionFile="$HOME/Library/Developer/Toolchains/Malterlib.xctoolchain.llvm_version"
	ToolchainLLVMSpecificVersionFile="$HOME/Library/Developer/Toolchains/Malterlib.xctoolchain.llvm_${IntegerClangVersion}_versiontime"

	if ! [ -d "$HOME/Library/Developer/Toolchains/Malterlib.xctoolchain" ] \
		|| ! [ -f "$ToolchainVersionFile" ] || (( `cat $ToolchainVersionFile` < $XCODE_VERSION_ACTUAL )) \
		|| \
		( \
			( ! [ -f "$ToolchainLLVMVersionFile" ] || (( "`cat $ToolchainLLVMVersionFile`" < "$IntegerClangVersion" ))) \
			|| (( "`cat $ToolchainLLVMVersionFile`" == "$IntegerClangVersion" )) && ( ! [ -f "$ToolchainLLVMSpecificVersionFile" ] || (( `cat $ToolchainLLVMSpecificVersionFile` < $ToolchainVersionTime ))) \
		); then
		echo Updating tool chain
		./Scripts/generatetoolchain.sh
		echo "$XCODE_VERSION_ACTUAL" > "$ToolchainVersionFile"
		echo "$IntegerClangVersion" > "$ToolchainLLVMVersionFile"
		echo "$ToolchainVersionTime" > "$ToolchainLLVMSpecificVersionFile"
	fi

	if [[ "$TOOLCHAIN_DIR" != "$HOME/Library/Developer/Toolchains/Malterlib.xctoolchain" ]]; then
		echo "error: Toolchain not correctly setup"
		echo "error: Go into Xcode Preferences->Components->Toolchains and select the 'Malterlib llvm' toolchain"
		exit 1
	fi

	popd > /dev/null
}

BuildClangVersion $ClangVersion
UpdateToolChain
