#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeWorkspace.sh Workspace Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="${1:-Tests}"
Platform="${2:-$HostPlatform}"
Architecture="${3:-$HostArchitecture}"
Config="${4:-Debug}"
BuildSystemDir="${5:-BuildSystem/Default}"

echo $XCodeBuildTool -workspace "$BuildSystemDir/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config"
$XCodeBuildTool -workspace "$BuildSystemDir/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config"

exit 0
