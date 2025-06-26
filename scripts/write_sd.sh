#!/bin/bash
cd "$(dirname "$0")"
set -e

IMAGE="output.img"

if [ ! -f "$IMAGE" ]; then
    echo "Error: $IMAGE not found!"
    exit 1
fi

TARGET_DEV=""
CONFIRM="no"
AUTO_CONFIRM="no"

while [[ $# -gt 0 ]]; do
  case $1 in
    --device)
      TARGET_DEV="$2"
      shift 2
      ;;
    --yes)
      AUTO_CONFIRM="yes"
      shift
      ;;
    *)
      shift
      ;;
  esac
done

if [ -z "$TARGET_DEV" ]; then
  echo "Usage: $0 --device /dev/sdX [--yes]"
  exit 1
fi

if [ ! -b "$TARGET_DEV" ]; then
  echo "Error: $TARGET_DEV is not a valid block device."
  exit 1
fi

echo "You are about to overwrite $TARGET_DEV with $IMAGE."

if [ "$AUTO_CONFIRM" != "yes" ]; then
  read -rp "Are you sure? This will ERASE ALL DATA on $TARGET_DEV. Type 'yes' to continue: " CONFIRM
  if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 0
  fi
else
  echo "Auto-confirmation enabled, proceeding."
fi

echo "Unmounting all partitions on $TARGET_DEV..."
sudo umount "${TARGET_DEV}"* || true

echo "Flushing buffers..."
sudo blockdev --flushbufs "$TARGET_DEV"

echo "Writing image to $TARGET_DEV..."
sudo dd if="$IMAGE" of="$TARGET_DEV" bs=4M oflag=direct status=progress conv=fsync

echo "Syncing data..."
sync
sudo blockdev --flushbufs "$TARGET_DEV"

echo "Re-reading partition table..."
sudo partprobe "$TARGET_DEV"
sudo udevadm settle

echo "Done. $IMAGE written to $TARGET_DEV successfully."

