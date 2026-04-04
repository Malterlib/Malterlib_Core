#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -e

source "$DIR/Detect.sh"

set +e
"$MToolExecutable" "$@"
exit $?
