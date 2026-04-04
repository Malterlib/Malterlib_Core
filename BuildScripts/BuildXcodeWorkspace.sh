#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: BuildXcodeWorkspace.sh Workspace Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="${1:-Tests}"

source "$DIR/ResolveConfig.sh"

Platform="${2:-$MalterlibDefaultPlatform}"
Architecture="${3:-$MalterlibDefaultArchitecture}"
Config="${4:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${5:-${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}}"

echo xcodebuild -workspace "$BuildSystemDir/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config"
xcodebuild -workspace "$BuildSystemDir/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config" 2>&1 | MTool XcodeBuildFilter

exit 0
