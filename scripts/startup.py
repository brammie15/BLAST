# SPDX-License-Identifier: GPL-2.0-or-later

import time
import uuid
import socket
import os
import gpiod

from signalrcore.hub_connection_builder import HubConnectionBuilder

# === Read config from file ===
CONFIG_FILE = os.path.join(os.path.dirname(__file__), "script_config.txt")

def read_config(filename):
    config = {}
    try:
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if '=' in line:
                    key, val = line.split("=", 1)
                    config[key.strip()] = val.strip()
    except FileNotFoundError:
        print(f"Config file {filename} not found, using default values.")
    return config

config = read_config(CONFIG_FILE)

# === Configuratie (use config or defaults) ===
GPIO_PIN = int(config.get("GPIO_PIN", 17))  # LINE_OFFSET in gpiod terms
CHIP_PATH = config.get("CHIP_PATH", "/dev/gpiochip0")
HUB_URL = config.get("HUB_URL", "http://192.168.1.78:5000/DataCaptatieHub")
HUB_NAME = config.get("HUB_NAME", "DataCaptatieHub")
METHOD_NAME = config.get("METHOD_NAME", "BananaPiPinChanged")

# === Uniek apparaat-ID ===
def get_mac():
    mac = uuid.getnode()
    return ':'.join(f"{(mac >> ele) & 0xff:02x}" for ele in range(0, 8*6, 8))[::-1]

def get_name():
    return socket.gethostname()

DEVICE_ID = get_mac()
#DEVICE_ID = get_name()

# === SignalR Setup ===
hub_connection = HubConnectionBuilder()\
    .with_url(HUB_URL)\
    .with_automatic_reconnect({
        "type": "raw",
        "keep_alive_interval": 10,
        "reconnect_interval": 5,
        "max_attempts": 5
    })\
    .build()

def send_signal():
    payload = {
        "deviceId": DEVICE_ID,
        "pin": GPIO_PIN,
        "state": "HIGH"
    }
    try:
        hub_connection.send(METHOD_NAME, [payload])
        print(f"→ Signaal verzonden: {payload}")
    except Exception as e:
        print("Fout bij verzenden:", e)

# === GPIO Setup with gpiod ===
line_settings = gpiod.LineSettings(
    edge_detection=gpiod.line.Edge.RISING,
    bias=gpiod.line.Bias.PULL_DOWN
)

with gpiod.request_lines(
    CHIP_PATH,
    config={GPIO_PIN: line_settings},
    consumer="gpio-edge-watcher"
) as request:
    print(f"Verbonden met SignalR hub.")
    hub_connection.start()
    print(f"Wachten op rising edge op GPIO lijn {GPIO_PIN}...")

    try:
        while True:
            # wait_edge_events(timeout) waits in seconds, or None for infinite
            if request.wait_edge_events():
                events = request.read_edge_events()
                for event in events:
                    print("events: " +str(event) + "Event Type: " + str(event.event_type))
                    if event.event_type == gpiod.EdgeEvent.Type.RISING_EDGE:
                        print(f"Rising edge gedetecteerd op lijn {event.line_offset}!")
                        send_signal()
            else:
                print("Geen event gedetecteerd in de laatste 10 seconden.")
    except KeyboardInterrupt:
        print("Beëindigen...")
    finally:
        hub_connection.stop()
        print("Opgeruimd en klaar.")
