#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

DEVICES="$PROJECT_DIR/scripts/devices.json"

# --------------------------------------------------
# Validate arguments
# --------------------------------------------------

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <target>"
    echo
    echo "Available targets:"
    jq -r '.targets | keys[]' "$DEVICES"
    exit 1
fi

TARGET="$1"

if ! jq -e --arg t "$TARGET" \
    '.targets[$t]' "$DEVICES" >/dev/null; then
    echo "Error: Unknown target '$TARGET'."
    exit 1
fi

# --------------------------------------------------
# Read compilation server configuration
# --------------------------------------------------

HOST="$(jq -r '.compile_server.host' "$DEVICES")"
USER="$(jq -r '.compile_server.user' "$DEVICES")"
PORT="$(jq -r '.compile_server.port // 22' "$DEVICES")"

REMOTE_PROJECT="$(jq -r '.compile_server.project_dir' "$DEVICES")"
PRESET="$(jq -r --arg t "$TARGET" '.targets[$t].build_preset' "$DEVICES")"

mapfile -t BUILD_ARGS < <(
    jq -r '.compile_server.build_args[]?' "$DEVICES"
)

REMOTE="$USER@$HOST"

# --------------------------------------------------
# Validate prerequisites
# --------------------------------------------------

for TOOL in ssh rsync jq; do
    if ! command -v "$TOOL" >/dev/null 2>&1; then
        echo "Error: Required tool '$TOOL' not found."
        exit 1
    fi
done

if [[ -z "${SSH_AUTH_SOCK:-}" || ! -S "$SSH_AUTH_SOCK" ]]; then
    echo "Error: SSH agent is not available."
    echo "Load your SSH key with ssh-add first."
    exit 1
fi

if [[ "$REMOTE_PROJECT" != /* ]]; then
    echo "Error: Compilation server project_dir must be absolute."
    exit 1
fi

# --------------------------------------------------
# Display configuration
# --------------------------------------------------

echo "========================================"
echo " Push / Compile / Deploy"
echo "========================================"

echo "Local project:      $PROJECT_DIR"
echo "Compilation server: $REMOTE"
echo "Remote project:     $REMOTE_PROJECT"
echo "Build preset:       $PRESET"
echo "Deployment target:  $TARGET"

if [[ ${#BUILD_ARGS[@]} -gt 0 ]]; then
    printf 'Build arguments:    '
    printf '%q ' "${BUILD_ARGS[@]}"
    echo
fi

echo "========================================"

# --------------------------------------------------
# Prepare remote directory
# --------------------------------------------------

printf -v REMOTE_PROJECT_Q '%q' "$REMOTE_PROJECT"

echo
echo "Preparing compilation server..."

ssh -A -p "$PORT" "$REMOTE" \
    "mkdir -p -- $REMOTE_PROJECT_Q"

# --------------------------------------------------
# Synchronize source code
# --------------------------------------------------

echo
echo "Synchronizing source code..."

rsync \
    --archive \
    --compress \
    --verbose \
    --human-readable \
    --itemize-changes \
    --partial \
    --protect-args \
    --exclude='/.git/' \
    --exclude='/build/' \
    --exclude='/containers/' \
    --exclude='/.cache/' \
    -e "ssh -A -p $PORT" \
    "$PROJECT_DIR/" \
    "$REMOTE:$REMOTE_PROJECT/"

echo
echo "Source synchronization successful."

# --------------------------------------------------
# Build and deploy remotely
# --------------------------------------------------

echo
echo "Starting remote build..."

REMOTE_COMMAND="cd -- $REMOTE_PROJECT_Q && ./scripts/compile.sh"

# Build preset
printf -v ARG_Q '%q' "$PRESET"
REMOTE_COMMAND+=" $ARG_Q"

# Additional build arguments
for ARG in "${BUILD_ARGS[@]}"; do
    printf -v ARG_Q '%q' "$ARG"
    REMOTE_COMMAND+=" $ARG_Q"
done

# Deploy only after successful build
printf -v TARGET_Q '%q' "$TARGET"
REMOTE_COMMAND+=" && ./scripts/deploy.sh $TARGET_Q"

ssh -A -p "$PORT" "$REMOTE" "$REMOTE_COMMAND"

# --------------------------------------------------
# Complete
# --------------------------------------------------

echo
echo "========================================"
echo " Push / Compile / Deploy successful"
echo "========================================"
