#!/bin/bash
set -e

IMAGE="output.img"
DEVICE="/dev/sdc"

if [ ! -f "$IMAGE" ]; then
  echo "Error: $IMAGE not found!"
  exit 1
fi

if [ ! -b "$DEVICE" ]; then
  echo "Error: $DEVICE is not a valid block device."
  exit 1
fi

echo "You are about to overwrite $DEVICE with $IMAGE."
read -rp "Type 'yes' to continue: " CONFIRM
if [ "$CONFIRM" != "yes" ]; then
  echo "Aborted."
  exit 0
fi

echo "Unmounting all partitions on $DEVICE..."
sudo umount "${DEVICE}"* || true

echo "Flushing buffers..."
sudo blockdev --flushbufs "$DEVICE"

echo "Writing image to $DEVICE..."
sudo dd if="$IMAGE" of="$DEVICE" bs=4M oflag=direct status=progress conv=fsync

echo "Syncing data..."
sync
sudo blockdev --flushbufs "$DEVICE"

echo "Re-reading partition table..."
sudo partprobe "$DEVICE"
sudo udevadm settle

echo "Done. Image successfully written to $DEVICE."
