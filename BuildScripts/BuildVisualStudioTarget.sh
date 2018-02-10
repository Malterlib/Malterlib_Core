#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildVisualStudioTarget.sh Workspace Target Platform Architecture Configuration 

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

CallDirect msbuild.exe "\"BuildSystem/Default/${1}.sln\"" /nodereuse:false /m /v:m "\"/target:$2\"" "\"/property:Platform=$3 - $4\"" "\"/property:Configuration=$5\""
CheckErrors

exit 0
