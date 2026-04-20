
###### make sure this is installed in the Pi first!!! ##############
 # pip install bleak paho-mqtt
####################################################################

import asyncio
import json
import ssl
import paho.mqtt.client as mqtt
from bleak import BleakClient

# --- Config ---
ESP32_ADDRESS       = "XX:XX:XX:XX:XX:XX"   # BLE MAC from scan
CHAR_UUID           = "abcd1234-ab12-ab12-ab12-abcdef012345"
AWS_ENDPOINT        = "YOUR-ENDPOINT.iot.us-east-1.amazonaws.com"
MQTT_TOPIC          = "sensors/esp32/data"
CERT_DIR            = "/home/pi/certs"

# --- MQTT setup ---
mqtt_client = mqtt.Client(client_id="rpi-ble-bridge")
mqtt_client.tls_set(
    ca_certs   = f"{CERT_DIR}/AmazonRootCA1.pem",
    certfile   = f"{CERT_DIR}/device-cert.pem",
    keyfile    = f"{CERT_DIR}/private.pem",
    tls_version= ssl.PROTOCOL_TLSv1_2
)
mqtt_client.connect(AWS_ENDPOINT, port=8883)
mqtt_client.loop_start()

# --- BLE notification handler ---
def handle_notification(sender, data):
    payload = data.decode("utf-8")
    print(f"BLE received: {payload}")
    result = mqtt_client.publish(MQTT_TOPIC, payload, qos=1)
    print(f"MQTT publish: {'OK' if result.rc == 0 else 'FAILED'}")

# --- Main BLE loop ---
async def main():
    async with BleakClient(ESP32_ADDRESS) as client:
        print(f"Connected to {ESP32_ADDRESS}")
        await client.start_notify(CHAR_UUID, handle_notification)
        print("Listening for notifications (Ctrl+C to stop)...")
        while True:
            await asyncio.sleep(1)

if __name__ == "__main__":
    asyncio.run(main())
