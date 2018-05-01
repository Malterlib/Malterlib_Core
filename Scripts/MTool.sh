#!/bin/bash
# Copyright © 2015-2018 Nonna Holding AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -e

source "$DIR/Detect.sh"

set +e
"$MToolExecutable" "$@"
MToolExit=$?
set -e

if [[ $MToolExit != 0 ]] ; then
	exit $MToolExit
fi
