#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Source from the root of the checkout. Sources the settings of the build system in MalterlibBuildSystemDir, BuildSystem/Default
# unless it is set, and exports MalterlibBuildSystemDir for what runs after. BuildSystem/Default falls back to
# BuildSystem/SharedBuildSettings.sh. MalterlibDisableBuildSystemGeneration keeps a value that it had.

BuildSystemSettingsDisableGeneration="${MalterlibDisableBuildSystemGeneration-}"
BuildSystemSettingsDisableGenerationSet="${MalterlibDisableBuildSystemGeneration+set}"

export MalterlibBuildSystemDir="${MalterlibBuildSystemDir:-BuildSystem/Default}"

# Otherwise a build that another build runs would keep the settings of that build system
unset MalterlibBuildSystemGenerateCommand MalterlibGeneratedBuildSystemDir

if [ -f "$MalterlibBuildSystemDir/SharedBuildSettings.sh" ]; then
	source "$MalterlibBuildSystemDir/SharedBuildSettings.sh"
elif [ -f ./BuildSystem/SharedBuildSettings.sh ] && { [ "$MalterlibBuildSystemDir" = "BuildSystem/Default" ] || [ "$MalterlibBuildSystemDir" = "$PWD/BuildSystem/Default" ]; }; then
	source ./BuildSystem/SharedBuildSettings.sh
fi

if [ -n "$BuildSystemSettingsDisableGenerationSet" ]; then
	export MalterlibDisableBuildSystemGeneration="$BuildSystemSettingsDisableGeneration"
fi

# The build scripts refuse to build a build system that generates import caches (MalterlibGenerateImportCaches)
BuildSystemRefuseImportCaches()
{
	if [[ "$MalterlibGenerateImportCaches" == "true" ]]; then
		echo "The build system in '$MalterlibBuildSystemDir' generates import caches (MalterlibGenerateImportCaches), so it is not built." >&2
		echo "Remove the setting from its UserSettings.MSettings to build." >&2
		exit 1
	fi
}
