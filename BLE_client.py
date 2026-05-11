import asyncio
import json
import logging
import signal
import sys
from datetime import datetime

from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient
from bleak import BleakClient, BleakError

# ─────────────────────────────────────────────
#  CONFIGURATION
# ─────────────────────────────────────────────
ESP32_ADDRESS = "F4:2D:C9:71:10:AE"                        # BLE MAC of your ESP32
CHAR_UUID     = "abcd1234-ab12-ab12-ab12-abcdef012345"     # BLE characteristic UUID

HOST          = "a34l9n2bhgxips-ats.iot.us-east-2.amazonaws.com"  # your AWS endpoint
CLIENT_ID     = "GianPi"
TOPIC         = "IoTProject"

CERT_DIR      = "/home/giancarlos/cert/"
CA_CERT       = f"{CERT_DIR}RootCA1.pem"
PRIVATE_KEY   = f"{CERT_DIR}GianPi-private.pem.key"
DEVICE_CERT   = f"{CERT_DIR}GianPi-cert.pem.crt"

# Reliability tuning (mirrors the SenseHat reference code)
BLE_RETRY_DELAY_SEC = 5
# ─────────────────────────────────────────────

# ── Logging ──────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler("/home/giancarlos/hvac_bridge.log"),
    ],
)
log = logging.getLogger(__name__)

# ── Graceful-shutdown flag ────────────────────
_shutdown = asyncio.Event()

def _handle_signal(*_):
    log.info("Shutdown signal received.")
    _shutdown.set()


# ─────────────────────────────────────────────
#  AWS IoT CLIENT  
# ─────────────────────────────────────────────

def build_aws_client() -> AWSIoTMQTTClient:
   
    # Configure and connect the AWSIoTMQTTClient.
    # Mirrors the SenseHat reference code configuration exactly.
    
    client = AWSIoTMQTTClient(CLIENT_ID)
    client.configureEndpoint(HOST, 8883)
    client.configureCredentials(CA_CERT, PRIVATE_KEY, DEVICE_CERT)

    # ── Same settings as the SenseHat reference code ──
    client.configureAutoReconnectBackoffTime(1, 32, 20)  # min=1s, max=32s, stable=20s
    client.configureOfflinePublishQueueing(-1)           # infinite offline queue
    client.configureDrainingFrequency(2)                 # drain at 2 Hz
    client.configureConnectDisconnectTimeout(10)         # 10 sec
    client.configureMQTTOperationTimeout(5)              # 5 sec

    log.info(f"Connecting to AWS IoT Core at {HOST} …")
    client.connect()
    log.info("AWS IoT Core connected.")
    return client


# ─────────────────────────────────────────────
#  PAYLOAD HELPERS
# ─────────────────────────────────────────────

# Sequence counter (same as loopCount in reference code)
_sequence = 0

def build_payload(raw: str) -> str:
   
    # Parse the raw BLE string and build an enriched JSON payload.
    # Matches the payload structure from the SenseHat reference code:
    #  { sequence, timestamp, ...sensor fields }

    # The ESP32 should send JSON like:
    # {"temp": 22.5, "humidity": 60, "pressure": 1013}
    # If it sends a plain value, it's wrapped as {"value": <raw>}.
   
    global _sequence

    try:
        sensor_data = json.loads(raw)
        if not isinstance(sensor_data, dict):
            sensor_data = {"value": sensor_data}
    except (json.JSONDecodeError, ValueError):
        sensor_data = {"raw": raw}

    payload = {
        "sequence":  _sequence,
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),  # pandas-friendly
        "device_id": CLIENT_ID,
        **sensor_data,   # spread in temp / humidity / pressure / etc.
    }
    _sequence += 1
    return json.dumps(payload)


# ─────────────────────────────────────────────
#  BLE BRIDGE
# ─────────────────────────────────────────────

async def run_ble_bridge(aws_client: AWSIoTMQTTClient) -> None:
    
    # Connect to the ESP32 over BLE and stream every notification to AWS.
    # Automatically retries the BLE connection if it drops.

    def handle_notification(sender, data: bytearray) -> None:
        try:
            raw = data.decode("utf-8").strip()
        except UnicodeDecodeError:
            log.warning(f"Non-UTF-8 BLE data: {data.hex()}")
            return

        message_json = build_payload(raw)
        aws_client.publish(TOPIC, message_json, 1)
        log.info(f"Published to {TOPIC}: {message_json}")

    while not _shutdown.is_set():
        try:
            log.info(f"Attempting BLE connection to {ESP32_ADDRESS} …")
            async with BleakClient(ESP32_ADDRESS) as ble:
                log.info(f"BLE connected to {ESP32_ADDRESS}")
                await ble.start_notify(CHAR_UUID, handle_notification)
                log.info("Subscribed — streaming to AWS IoT Core. Press Ctrl+C to stop.")

                while not _shutdown.is_set() and ble.is_connected:
                    await asyncio.sleep(1)

                await ble.stop_notify(CHAR_UUID)

        except BleakError as exc:
            log.error(f"BLE error: {exc}")
        except asyncio.CancelledError:
            break
        except Exception as exc:
            log.exception(f"Unexpected error: {exc}")

        if not _shutdown.is_set():
            log.info(f"Retrying BLE in {BLE_RETRY_DELAY_SEC}s …")
            await asyncio.sleep(BLE_RETRY_DELAY_SEC)


# ─────────────────────────────────────────────
#  ENTRY POINT
# ─────────────────────────────────────────────

async def async_main() -> None:
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, _handle_signal)

    log.info("=== Transportation HVAC IoT — Pi BLE Bridge starting ===")

    aws_client = build_aws_client()

    try:
        await run_ble_bridge(aws_client)
    finally:
        log.info("Disconnecting from AWS IoT Core …")
        aws_client.disconnect()
        log.info("Bridge shut down cleanly.")


if __name__ == "__main__":
    asyncio.run(async_main())
