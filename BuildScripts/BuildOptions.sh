#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The options of build and build-target, which can be anywhere among the arguments:
#   --output-directory <Directory>  The generated build system to build in, relative to the root of the checkout.
#                                   BuildSystem/Default unless it is given, as for generate
#   --wait-for-lock                 Wait for another build of the same configuration to finish, instead of refusing
#                                   to build. Builds that another build runs, as of host tools, always wait
#   --lock-timeout <Seconds>        Wait for another build of the same configuration for at most this many seconds,
#                                   which implies --wait-for-lock; negative sets no limit. Builds that it runs use the
#                                   same timeout
# BuildOptionsParse exports what the options set and leaves the other arguments in BuildOptionsArguments.

BuildOptionsParse()
{
	BuildOptionsArguments=()

	while [ $# -gt 0 ]; do
		case "$1" in
			--output-directory)
				if [ $# -lt 2 ]; then
					echo "--output-directory needs a directory" >&2
					return 1
				fi
				export MalterlibBuildSystemDir="$2"
				shift 2
				;;
			--output-directory=*)
				export MalterlibBuildSystemDir="${1#*=}"
				shift
				;;
			--wait-for-lock)
				export MalterlibBuildLockWait=true
				shift
				;;
			--lock-timeout)
				if [ $# -lt 2 ]; then
					echo "--lock-timeout needs a number of seconds" >&2
					return 1
				fi
				export MalterlibBuildLockTimeout="$2"
				shift 2
				;;
			--lock-timeout=*)
				export MalterlibBuildLockTimeout="${1#*=}"
				shift
				;;
			*)
				BuildOptionsArguments+=("$1")
				shift
				;;
		esac
	done
}

# Generates the build system in MalterlibBuildSystemDir for a workspace, unless generation is disabled, and reads its settings
# again. A build system that has not been generated yet has no settings with the command, and is generated for Ninja.
BuildOptionsGenerate()
{
	local Workspace="$1"

	if [[ "$MalterlibDisableBuildSystemGeneration" == "true" ]]; then
		return 0
	fi

	# Builds of two configurations overlap and both generate, so this waits for another generation instead of refusing. The
	# MTool that runs can be older than the one that wrote the settings, as after switching to an older branch, and MTools before
	# bootstrap version 6 refuse the option but ignore the variable
	local BootstrapVersion
	BootstrapVersion="$(cat "$MToolDirectory/Bootstrap.version" 2>/dev/null)"
	local WaitForLock=""
	if [[ "$BootstrapVersion" =~ ^[0-9]+$ ]] && (( BootstrapVersion >= 6 )); then
		WaitForLock="--wait-for-lock"
	fi

	if [ -n "$MalterlibBuildSystemGenerateCommand" ]; then
		MalterlibBuildLockWait=true eval "$MalterlibBuildSystemGenerateCommand $WaitForLock --no-signal-changed \"$Workspace\""
	else
		MalterlibBuildLockWait=true ./mib generate $WaitForLock --generator Ninja --output-directory "$MalterlibBuildSystemDir" --no-signal-changed "$Workspace"
	fi

	# A generation can change the settings that the build uses, as the generator
	source "$BuildOptionsScriptDir/BuildSystemSettings.sh"
}

BuildOptionsScriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
