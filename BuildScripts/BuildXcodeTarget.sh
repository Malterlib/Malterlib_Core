#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeTarget.sh Workspace Target Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="${1:-Tests}"
Target="${2:-Build All}"
Platform="${3:-$HostPlatform}"
Architecture="${4:-$HostArchitecture}"
Config="${5:-Debug}"
BuildSystemDir="${6:-BuildSystem/Default}"

echo $XCodeBuildTool -workspace "${BuildSystemDir}/${Workspace}.xcworkspace" -scheme "$Target $Platform $Architecture $Config"
$XCodeBuildTool -workspace "${BuildSystemDir}/${Workspace}.xcworkspace" -scheme "$Target $Platform $Architecture $Config"

exit 0

