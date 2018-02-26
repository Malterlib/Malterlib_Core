#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeTarget.sh Workspace Target Platform Architecture Configuration 

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/local/bin:/opt/local/sbin:/usr/local/bin:$PATH"

$XCodeBuildTool -workspace "BuildSystem/Default/${1}.xcworkspace" -scheme "$2 $3 $4 $5"
CheckErrors

exit 0

