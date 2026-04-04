#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: BuildXcodeTarget.sh Workspace Target Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="${1:-Tests}"
Target="${2:-Build All}"

source "$DIR/ResolveConfig.sh"

Platform="${3:-$MalterlibDefaultPlatform}"
Architecture="${4:-$MalterlibDefaultArchitecture}"
Config="${5:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${6:-${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}}"

echo xcodebuild -workspace "${BuildSystemDir}/${Workspace}.xcworkspace" -scheme "$Target $Platform $Architecture $Config"
xcodebuild -workspace "${BuildSystemDir}/${Workspace}.xcworkspace" -scheme "$Target $Platform $Architecture $Config" 2>&1 | MTool XcodeBuildFilter

exit 0

