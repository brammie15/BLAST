#!/bin/bash
cd "$(dirname "$0")"
set -e

WORK_IMAGE="output.img"
MOUNT_DIR="/mnt/img"
CONFIG_FILE="script_config.txt"

cleanup() {
    echo "Cleaning up mounts..."
    umount "$MOUNT_DIR" 2>/dev/null || true
}
trap cleanup EXIT

echo "Attaching loop device..."
LOOP_DEV=$(losetup --show -f "$WORK_IMAGE")
DEVICE_NAME=$(basename "$LOOP_DEV")

# Map the partitions
kpartx -av "$LOOP_DEV"
sleep 2

MAPPED_DEV="/dev/mapper/${DEVICE_NAME}p1"

echo "Mounting root filesystem..."
mkdir -p "$MOUNT_DIR"
mount "$MAPPED_DEV" "$MOUNT_DIR"

echo "Copying updated config file..."
cp "$CONFIG_FILE" "$MOUNT_DIR/root/$CONFIG_FILE"

echo "Syncing and unmounting..."
sync

kpartx -d "$LOOP_DEV"
losetup -d "$LOOP_DEV"
rmdir "$MOUNT_DIR"

echo "Done. $CONFIG_FILE copied to root of $WORK_IMAGE"
