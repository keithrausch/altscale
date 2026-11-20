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
# Read configuration
# --------------------------------------------------

HOST="$(jq -r --arg t "$TARGET" '.targets[$t].host' "$DEVICES")"
USER="$(jq -r --arg t "$TARGET" '.targets[$t].user' "$DEVICES")"
PORT="$(jq -r --arg t "$TARGET" '.targets[$t].port // 22' "$DEVICES")"
DEST="$(jq -r --arg t "$TARGET" '.targets[$t].destination' "$DEVICES")"

REMOTE="$USER@$HOST"

# --------------------------------------------------
# Validate SSH agent
# --------------------------------------------------

if [[ -z "${SSH_AUTH_SOCK:-}" || ! -S "$SSH_AUTH_SOCK" ]]; then
    echo "Error: SSH agent is not available."
    exit 1
fi
# --------------------------------------------------
# Prepare artifact list (recursive glob support)
# --------------------------------------------------

ARTIFACT_LIST="$(mktemp)"
trap 'rm -f "$ARTIFACT_LIST"' EXIT

shopt -s globstar nullglob

while IFS= read -r PATTERN; do

    [[ -z "$PATTERN" ]] && continue

    # Reject absolute paths and parent-directory traversal.
    if [[ "$PATTERN" == /* || "$PATTERN" =~ (^|/)\.\.(/|$) ]]; then
        echo "Error: Invalid artifact path: $PATTERN"
        exit 1
    fi

    MATCHES=( "$PROJECT_DIR"/$PATTERN )

    if [[ ${#MATCHES[@]} -eq 0 ]]; then
        echo "Error: Artifact pattern matched nothing: $PATTERN"
        exit 1
    fi

    for FILE in "${MATCHES[@]}"; do

        if [[ ! -e "$FILE" ]]; then
            continue
        fi

        RELATIVE="${FILE#"$PROJECT_DIR"/}"

        printf '%s\n' "$RELATIVE" >> "$ARTIFACT_LIST"

    done

done < <(
    jq -r --arg t "$TARGET" \
        '.targets[$t].artifacts[]' "$DEVICES"
)

if [[ ! -s "$ARTIFACT_LIST" ]]; then
    echo "Error: No artifacts selected."
    exit 1
fi

# --------------------------------------------------
# Deploy
# --------------------------------------------------

echo "========================================"
echo "Deploying: $TARGET"
echo "Destination: $REMOTE:$DEST"
echo "========================================"

ssh -A -p "$PORT" "$REMOTE" \
    "mkdir -p -- '$DEST'"

echo
echo "Transferring artifacts..."

rsync \
    --archive \
    --recursive \
    --verbose \
    --human-readable \
    --itemize-changes \
    --partial \
    --protect-args \
    --relative \
    --files-from="$ARTIFACT_LIST" \
    -e "ssh -A -p $PORT" \
    "$PROJECT_DIR/" \
    "$REMOTE:$DEST/"

echo
echo "Deployment completed successfully."
