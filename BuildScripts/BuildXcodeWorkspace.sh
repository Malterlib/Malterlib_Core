#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeWorkspace.sh Workspace Platform Architecture Configuration 

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/local/sbin:/usr/local/bin:/opt/local/bin:$PATH"

$XCodeBuildTool -workspace "BuildSystem/Default/$1.xcworkspace" -scheme "Build All $2 $3 $4"
CheckErrors

exit 0
