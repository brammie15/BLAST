#import RPi.GPIO as GPIO  # GPIO module disabled for testing
import time
import uuid
import socket
from signalrcore.hub_connection_builder import HubConnectionBuilder

# === Configuratie ===
GPIO_PIN = 10  # voorbeeld: pin 10 (GPIO15)
HUB_URL = "http://192.168.1.78:5000/DataCaptatieHub"
HUB_NAME = "DataCaptatieHub"
METHOD_NAME = "BananaPiPinChanged"

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

# === GPIO Setup (uitgeschakeld) ===
# GPIO.setmode(GPIO.BCM)
# GPIO.setup(GPIO_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)

# def gpio_callback(channel):
#     if GPIO.input(channel) == GPIO.HIGH:
#         send_signal()

# GPIO.add_event_detect(GPIO_PIN, GPIO.RISING, callback=gpio_callback, bouncetime=200)

# === Start verbinding ===
hub_connection.start()
print("Verbonden met SignalR hub.")

try:
    print("Simuleer handmatig signaalverzending... (CTRL+C om te stoppen)")
    while True:
        time.sleep(5)
        send_signal()  # Simuleer een signaal elke 5 seconden
except KeyboardInterrupt:
    print("Beëindigen...")
finally:
    hub_connection.stop()
    # GPIO.cleanup()
    print("Opgeruimd en klaar.")

