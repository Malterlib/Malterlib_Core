#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeWorkspace.sh Workspace Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="${1:-Tests}"
Platform="${2:-$HostPlatform}"
Architecture="${3:-$HostArchitecture}"
Config="${4:-Debug}"
BuildSystemDir="${5:-BuildSystem/Default}"

echo xcodebuild -workspace "$BuildSystemDir/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config"
xcodebuild -workspace "$BuildSystemDir/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config" 2>&1 | MTool XcodeBuildFilter

exit 0
