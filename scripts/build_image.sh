#!/bin/bash
cd "$(dirname "$0")"

set -e

IMAGE_XZ=$(ls *.img.xz)
BASE_IMAGE="${IMAGE_XZ%.xz}"
WORK_IMAGE="output.img"
MOUNT_DIR="/mnt/img"
SCRIPT_NAME="startup.py"
CONFIG_FILE="config.txt"
IMAGE_EXTENSION_SIZE="500M"

echo "Reading config.txt..."
source "$CONFIG_FILE"

cleanup() {
    echo "Cleaning up mounts..."
    umount "$MOUNT_DIR/dev/pts" 2>/dev/null || true
    umount "$MOUNT_DIR/dev" 2>/dev/null || true
    umount "$MOUNT_DIR/proc" 2>/dev/null || true
    umount "$MOUNT_DIR/sys" 2>/dev/null || true
    umount "$MOUNT_DIR" 2>/dev/null || true
}
trap cleanup EXIT

# Check if unpacked output image already exists
if [ -f "$WORK_IMAGE" ]; then
    echo "Warning: $WORK_IMAGE already exists. Deleting it..."
    rm -f "$WORK_IMAGE"
fi

if [ -f "$BASE_IMAGE" ]; then
    echo "$BASE_IMAGE already exists. Skipping extraction."
else
    echo "Unpacking base image..."
    xz -dk "$IMAGE_XZ"
fi
cp "$BASE_IMAGE" "$WORK_IMAGE"

echo "Expanding image by $IMAGE_EXTENSION_SIZE..."
truncate -s +$IMAGE_EXTENSION_SIZE "$WORK_IMAGE"

echo "Attaching loop device..."
LOOP_DEV=$(losetup --show -f "$WORK_IMAGE")
DEVICE_NAME=$(basename "$LOOP_DEV")

parted "$LOOP_DEV" ---pretend-input-tty <<EOF
resizepart 1 100%
Yes
quit
EOF

# Map the partitions
kpartx -av "$LOOP_DEV"
sleep 2

MAPPED_DEV="/dev/mapper/${DEVICE_NAME}p1"

echo "Running fsck and resizing filesystem on $MAPPED_DEV"
e2fsck -fy "$MAPPED_DEV"
resize2fs "$MAPPED_DEV"

echo "Mounting root filesystem..."
mkdir -p "$MOUNT_DIR"
mount "$MAPPED_DEV" "$MOUNT_DIR"

echo "Binding system directories..."
mount --rbind /dev "$MOUNT_DIR/dev"
mount --bind /proc "$MOUNT_DIR/proc"
mount --bind /sys "$MOUNT_DIR/sys"
mount --bind /dev/pts "$MOUNT_DIR/dev/pts"

echo "Fixing DNS"
rm -f "$MOUNT_DIR/etc/resolv.conf"
cp -f /etc/resolv.conf "$MOUNT_DIR/etc/resolv.conf"

echo "Injecting QEMU for ARM emulation..."
cp /usr/bin/qemu-aarch64-static "$MOUNT_DIR/usr/bin/"

echo "Copying files..."
cp "$SCRIPT_NAME" "$MOUNT_DIR/root/$SCRIPT_NAME"
chmod +x "$MOUNT_DIR/root/$SCRIPT_NAME"
cp "script_config.txt" "$MOUNT_DIR/root/script_config.txt"

echo "Setting root password..."
chroot "$MOUNT_DIR" bash -c "echo 'root:root' | chpasswd"

echo "Installing Python and required packages..."
chroot "$MOUNT_DIR" apt-get update
chroot "$MOUNT_DIR" apt-get install -y python3 python3-pip python3-dev 
chroot "$MOUNT_DIR" apt-get install -y gcc-arm-linux-gnueabihf
chroot "$MOUNT_DIR" pip install --no-cache-dir signalrcore --break-system-packages

chroot "$MOUNT_DIR" pip install gpiod --break-system-packages


echo "Enabling autologin..."
mkdir -p "$MOUNT_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<EOF > "$MOUNT_DIR/etc/systemd/system/getty@tty1.service.d/override.conf"
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin root --noclear %I \$TERM
EOF

echo "Creating systemd service for startup script..."
cat <<EOF > "$MOUNT_DIR/etc/systemd/system/myscript.service"
[Unit]
Description=Run Python Script at Boot
After=network.target

[Service]
ExecStart=/usr/bin/python3 /root/$SCRIPT_NAME
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

chroot "$MOUNT_DIR" systemctl enable myscript.service

echo "Adding Wi-Fi settings..."
NM_CFG="$MOUNT_DIR/etc/netplan/30-wifis-dhcp.yaml"
mkdir -p "$(dirname "$NM_CFG")"
cat <<EOF > "$NM_CFG"
# Created by build script
network:
  version: 2
  wifis:
    wlan0:
      dhcp4: yes
      access-points:
        "$WIFI_SSID":
          password: "$WIFI_PASS"
EOF
chmod 600 "$NM_CFG"

echo "Cleaning apt cache..."
chroot "$MOUNT_DIR" apt-get clean
chroot "$MOUNT_DIR" rm -rf /var/lib/apt/lists/*

# Wait 5 seconds to be sure that everything is done
sleep 5

echo "Unmounting and cleaning up..."
#umount -l "$MOUNT_DIR/dev/pts"
#umount -l "$MOUNT_DIR/dev"
#umount -l "$MOUNT_DIR/proc"
#umount -l "$MOUNT_DIR/sys"
#umount -l "$MOUNT_DIR"

kpartx -d "$LOOP_DEV"
losetup -d "$LOOP_DEV"
rmdir "$MOUNT_DIR"

echo "Done. Image is saved as: $WORK_IMAGE"


