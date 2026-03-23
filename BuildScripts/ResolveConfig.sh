#!/bin/bash
# Copyright © 2026 Unbroken AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Resolves MalterlibDefaultPlatform, MalterlibDefaultArchitecture, and
# MalterlibDefaultConfiguration from ConfigStore for the current Workspace.
#
# Source this script after setting Workspace. It updates the MalterlibDefault*
# variables so that callers can keep using ${arg:-$MalterlibDefault*} patterns.

ResolveConfigBuildSystemDir="${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}"
ResolveConfigDir="${ResolveConfigBuildSystemDir}/ConfigStore/${Workspace}/Configs"

if [ -d "$ResolveConfigDir" ]; then
	BestScore=-1
	BestPriority=-1

	for ConfigFile in "$ResolveConfigDir"/*.json; do
		[ -f "$ConfigFile" ] || continue

		IFS=$'\n' read -r -d '' FilePlatform FileArchitecture FileConfig FilePriority <<< "$(MTool --no-color ReadJson "$ConfigFile" platform architecture configuration configurationPriority 2>/dev/null)" || true

		Score=0
		if [ "$FilePlatform" = "$MalterlibDefaultPlatform" ]; then Score=$((Score + 1)); fi
		if [ "$FileArchitecture" = "$MalterlibDefaultArchitecture" ]; then Score=$((Score + 1)); fi
		case "$FileConfig" in
			"$MalterlibDefaultConfiguration"*) Score=$((Score + 1)) ;;
		esac

		if [ "$Score" -gt "$BestScore" ] || { [ "$Score" -eq "$BestScore" ] && [ "$FilePriority" -gt "$BestPriority" ]; }; then
			BestScore=$Score
			BestPriority=$FilePriority
			BestPlatform=$FilePlatform
			BestArchitecture=$FileArchitecture
			BestConfig=$FileConfig
		fi
	done

	if [ "$BestScore" -ge 0 ]; then
		export MalterlibDefaultPlatform="$BestPlatform"
		export MalterlibDefaultArchitecture="$BestArchitecture"
		export MalterlibDefaultConfiguration="$BestConfig"
	fi
fi
