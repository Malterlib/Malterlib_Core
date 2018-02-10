#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildVisualStudioWorkspace.sh Workspace Platform Architecture Configuration 

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"
source "$DIR/MSysFixup.sh"

source ./BuildSystem/SharedBuildSettings.sh

# Remove quotes around config if they exist.
Config="${4%\"}"
Config="${Config#\"}"

echo CallDirect msbuild.exe "\"BuildSystem/Default/${1}.sln"\" /nodereuse:false /m /v:m "\"/p:Platform=$2 - $3\"" "\"/p:Configuration=$Config\""
CallDirect msbuild.exe "\"BuildSystem/Default/${1}.sln"\" /nodereuse:false /m /v:m "\"/p:Platform=$2 - $3\"" "\"/p:Configuration=$Config\""
CheckErrors

exit 0
