#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec pkexec /usr/bin/bash "$SCRIPT_DIR/container.sh" gdb "$@"