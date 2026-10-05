#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Source from the root of the checkout. Sources the settings of the generated build system in MalterlibBuildSystemDir,
# which is BuildSystem/Default unless it is set, and exports MalterlibBuildSystemDir for what is run after.
# BuildSystem/SharedBuildSettings.sh is the one of BuildSystem/Default from before the build systems had their own.
# MalterlibDisableBuildSystemGeneration keeps a value that it had before.

BuildSystemSettingsDisableGeneration="${MalterlibDisableBuildSystemGeneration-}"
BuildSystemSettingsDisableGenerationSet="${MalterlibDisableBuildSystemGeneration+set}"

export MalterlibBuildSystemDir="${MalterlibBuildSystemDir:-BuildSystem/Default}"

# A build that another build runs inherits the settings of that build system
unset MalterlibBuildSystemGenerateCommand MalterlibGeneratedBuildSystemDir

if [ -f "$MalterlibBuildSystemDir/SharedBuildSettings.sh" ]; then
	source "$MalterlibBuildSystemDir/SharedBuildSettings.sh"
elif [ -f ./BuildSystem/SharedBuildSettings.sh ] && { [ "$MalterlibBuildSystemDir" = "BuildSystem/Default" ] || [ "$MalterlibBuildSystemDir" = "$PWD/BuildSystem/Default" ]; }; then
	source ./BuildSystem/SharedBuildSettings.sh
fi

if [ -n "$BuildSystemSettingsDisableGenerationSet" ]; then
	export MalterlibDisableBuildSystemGeneration="$BuildSystemSettingsDisableGeneration"
fi
