#!/usr/bin/env bash

set -euo pipefail

IMAGE="localhost/cpp-dev:latest"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

CONTAINERFILE="$PROJECT_DIR/containerfile"
CONTAINERS_DIR="$PROJECT_DIR/containers"
IMAGE_ARCHIVE="$CONTAINERS_DIR/cpp-dev.tar"
METADATA="$CONTAINERS_DIR/cpp-dev.info"

# --------------------------------------------------
# Arguments
# --------------------------------------------------

ROOT_REQUESTED=false

if [[ "${1:-}" == "--root" ]]; then
    ROOT_REQUESTED=true
    shift
fi

COMMAND=("$@")

IS_GDB=false

if [[ "${COMMAND[0]:-}" == "gdb" ]]; then
    IS_GDB=true
fi

# GDB must have exclusive use of stdout.
if [[ "$IS_GDB" == true ]]; then
    exec 3>&1
    exec 1>&2
fi

# --------------------------------------------------
# Execution context
# --------------------------------------------------

RUNNING_AS_ROOT=false
INTERACTIVE=false
PSEUDOTERMINAL=false

if [[ "$EUID" -eq 0 ]]; then
    RUNNING_AS_ROOT=true
fi

if [[ -t 0 && -t 1 ]]; then
    INTERACTIVE=true
fi

if [[ $# -eq 0 ]]; then
    EXECUTION_MODE="Interactive shell"
    PSEUDOTERMINAL=true
elif [[ -t 0 && -t 1 ]]; then
    EXECUTION_MODE="Command"
    PSEUDOTERMINAL=true
else
    EXECUTION_MODE="Command"
fi

if [[ "$PSEUDOTERMINAL" == true ]]; then
    TTY_OPTION="-it"
else
    TTY_OPTION="none"
fi

if [[ "$RUNNING_AS_ROOT" == true ]]; then
    PODMAN=(/usr/bin/podman)
elif [[ "$ROOT_REQUESTED" == true ]]; then
    PODMAN=(sudo /usr/bin/podman)
else
    PODMAN=(/usr/bin/podman)
fi

# --------------------------------------------------
# Diagnostics
# --------------------------------------------------

echo "========================================"
echo " Container execution"
echo "========================================"

printf "Project directory:  %s\n" "$PROJECT_DIR"
printf "Containerfile:      %s\n" "$CONTAINERFILE"
printf "Image:              %s\n" "$IMAGE"

echo
echo "Execution context:"
printf "  User:             %s\n" "$(id -un)"
printf "  UID:              %s\n" "$EUID"
printf "  Running as root:  %s\n" "$RUNNING_AS_ROOT"
printf "  Root requested:   %s\n" "$ROOT_REQUESTED"
printf "  Interactive:      %s\n" "$INTERACTIVE"
printf "  Pseudo-terminal:  %s\n" "$PSEUDOTERMINAL"
printf "  TTY option:       %s\n" "$TTY_OPTION"
printf "  Execution mode:   %s\n" "$EXECUTION_MODE"

echo
echo "User supplied arguments:"

for i in "${!COMMAND[@]}"; do
    printf '  [%d] %q\n' "$i" "${COMMAND[$i]}"
done

echo
echo "Podman execution:"
printf "  "
printf '%q ' "${PODMAN[@]}"
echo
echo "========================================"

# --------------------------------------------------
# Validate prerequisites
# --------------------------------------------------

if [[ ! -f "$CONTAINERFILE" ]]; then
    echo "Error: Containerfile not found: $CONTAINERFILE"
    exit 1
fi

mkdir -p "$CONTAINERS_DIR"

# --------------------------------------------------
# Check image metadata
# --------------------------------------------------

CURRENT_HASH="$(sha256sum "$CONTAINERFILE" | awk '{print $1}')"

SAVED_HASH=""
SAVED_IMAGE_ID=""

if [[ -f "$METADATA" ]]; then
    SAVED_HASH="$(sed -n 's/^containerfile_sha256=//p' "$METADATA")"
    SAVED_IMAGE_ID="$(sed -n 's/^image_id=//p' "$METADATA")"
fi

NEEDS_BUILD=false

if [[ ! -f "$IMAGE_ARCHIVE" ]]; then
    echo "Image archive missing."
    NEEDS_BUILD=true
elif [[ "$CURRENT_HASH" != "$SAVED_HASH" ]]; then
    echo "Containerfile has changed."
    NEEDS_BUILD=true
elif [[ -z "$SAVED_IMAGE_ID" ]]; then
    echo "Image metadata missing or invalid."
    NEEDS_BUILD=true
fi

# --------------------------------------------------
# Build and export image
# --------------------------------------------------

if [[ "$NEEDS_BUILD" == true ]]; then

    echo "Building development image..."

    "${PODMAN[@]}" build \
        --tag "$IMAGE" \
        --file "$CONTAINERFILE" \
        "$PROJECT_DIR"

    IMAGE_ID="$("${PODMAN[@]}" image inspect \
        --format '{{.Id}}' "$IMAGE")"

    TEMP_ARCHIVE="$(mktemp "$CONTAINERS_DIR/.cpp-dev.XXXXXX")"
    TEMP_METADATA="$(mktemp "$CONTAINERS_DIR/.cpp-dev-info.XXXXXX")"

    trap 'rm -f "${TEMP_ARCHIVE:-}" "${TEMP_METADATA:-}"' EXIT

    echo "Exporting image archive..."

    "${PODMAN[@]}" save \
        --format oci-archive \
        --output "$TEMP_ARCHIVE" \
        "$IMAGE"

    mv "$TEMP_ARCHIVE" "$IMAGE_ARCHIVE"

    printf 'containerfile_sha256=%s\nimage_id=%s\n' \
        "$CURRENT_HASH" \
        "$IMAGE_ID" > "$TEMP_METADATA"

    mv "$TEMP_METADATA" "$METADATA"

    SAVED_IMAGE_ID="$IMAGE_ID"

    trap - EXIT

    echo "Image archive updated."
fi

# --------------------------------------------------
# Ensure correct image is loaded
# --------------------------------------------------

LOCAL_IMAGE_ID=""

if "${PODMAN[@]}" image exists "$IMAGE"; then
    LOCAL_IMAGE_ID="$("${PODMAN[@]}" image inspect \
        --format '{{.Id}}' "$IMAGE")"
fi

if [[ "$LOCAL_IMAGE_ID" != "$SAVED_IMAGE_ID" ]]; then

    echo "Loading image archive..."

    if [[ -n "$LOCAL_IMAGE_ID" ]]; then
        "${PODMAN[@]}" image rm "$IMAGE"
    fi

    "${PODMAN[@]}" load --input "$IMAGE_ARCHIVE"

    LOCAL_IMAGE_ID="$("${PODMAN[@]}" image inspect \
        --format '{{.Id}}' "$IMAGE")"

    if [[ "$LOCAL_IMAGE_ID" != "$SAVED_IMAGE_ID" ]]; then
        echo "Error: Loaded image ID does not match metadata."
        exit 1
    fi
fi

# --------------------------------------------------
# Run container
# --------------------------------------------------

RUN_ARGS=(
    --rm
    --network=host

    # Device access
    --privileged
    --volume /dev:/dev

    # Debugging capabilities
    --cap-add=SYS_PTRACE
    --security-opt seccomp=unconfined

    # Project
    --volume "$PROJECT_DIR:$PROJECT_DIR:Z"
    --workdir "$PROJECT_DIR"
)

if [[ "$IS_GDB" == true ]]; then
    # GDB/MI needs stdin, but not an allocated terminal.
    RUN_ARGS+=(-i)
elif [[ "$PSEUDOTERMINAL" == true ]]; then
    RUN_ARGS+=(-it)
elif [[ -t 0 ]]; then
    RUN_ARGS+=(-i)
fi

if [[ ${#COMMAND[@]} -eq 0 ]]; then
    COMMAND=(/bin/bash)
fi

echo
echo "Launching container..."

if [[ "$IS_GDB" == true ]]; then

    set +e

    "${PODMAN[@]}" run \
        "${RUN_ARGS[@]}" \
        "$IMAGE" \
        "${COMMAND[@]}" >&3

    EXIT_CODE=$?

    echo "GDB container exit code: $EXIT_CODE" >&2

    exit "$EXIT_CODE"
fi

if "${PODMAN[@]}" run \
    "${RUN_ARGS[@]}" \
    "$IMAGE" \
    "${COMMAND[@]}"; then
    EXIT_CODE=0
else
    EXIT_CODE=$?
fi

printf 'Container exit code: %s\n' "$EXIT_CODE"

exit "$EXIT_CODE"
