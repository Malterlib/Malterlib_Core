#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The options of build and build-target, which can be anywhere among the arguments:
#   --output-directory <Directory>  The generated build system to build in, relative to the root of the checkout.
#                                   BuildSystem/Default unless it is given, as for generate
#   --wait-for-lock                 Wait for another build of the same configuration to finish, instead of refusing
#                                   to build. Builds that it runs, as of host tools, wait as well
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
			*)
				BuildOptionsArguments+=("$1")
				shift
				;;
		esac
	done
}

# Generates the build system in MalterlibBuildSystemDir for a workspace, unless generation is disabled. A build system
# that has not been generated yet has no settings with the command, and is generated for Ninja.
BuildOptionsGenerate()
{
	local Workspace="$1"

	if [[ "$MalterlibDisableBuildSystemGeneration" == "true" ]]; then
		return 0
	fi

	if [ -n "$MalterlibBuildSystemGenerateCommand" ]; then
		eval "$MalterlibBuildSystemGenerateCommand --no-signal-changed \"$Workspace\""
	else
		./mib generate --generator Ninja --output-directory "$MalterlibBuildSystemDir" --no-signal-changed "$Workspace"
		source "$BuildOptionsScriptDir/BuildSystemSettings.sh"
	fi
}

BuildOptionsScriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
