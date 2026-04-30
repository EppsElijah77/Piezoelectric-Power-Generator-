import asyncio
import json
import logging
from datetime import datetime
from bleak import BleakClient, BleakScanner
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

# ─────────────────────────────────────────────
# BLE CONFIG  (must match your ESP32-C6 code)
# ─────────────────────────────────────────────
DEVICE_NAME = "ESP32-C6 FootStep"
TX_UUID     = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  # ESP32 notifies on this

# ─────────────────────────────────────────────
# AWS IoT CONFIG  (reusing SenseHat certs)
# ─────────────────────────────────────────────
HOST      = "AWS.Placeholder"
CERT_PATH = "cert/"
CLIENT_ID = "Pi_400_BLE_Bridge"
TOPIC     = "ESP32/FootStep"          # change to any topic you like

# ─────────────────────────────────────────────
# LOGGING
# ─────────────────────────────────────────────
logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
log = logging.getLogger(__name__)

# ─────────────────────────────────────────────
# MQTT CLIENT SETUP
# ─────────────────────────────────────────────
mqtt_client = AWSIoTMQTTClient(CLIENT_ID)
mqtt_client.configureEndpoint(HOST, 8883)
mqtt_client.configureCredentials(
    f"{CERT_PATH}RootCA1.pem",
    f"{CERT_PATH}SenseHat_S25-private.pem.key",
    f"{CERT_PATH}SenseHat_S25-cert.pem.crt",
)
mqtt_client.configureAutoReconnectBackoffTime(1, 32, 20)
mqtt_client.configureOfflinePublishQueueing(-1)   # queue messages while offline
mqtt_client.configureDrainingFrequency(2)
mqtt_client.configureConnectDisconnectTimeout(10)
mqtt_client.configureMQTTOperationTimeout(5)

sequence = 0  # message counter

# ─────────────────────────────────────────────
# BLE NOTIFICATION HANDLER
# ─────────────────────────────────────────────
def handle_data(sender, data: bytearray):
    """
    Called every time the ESP32 sends a BLE notification.
    Parses the raw UTF-8 message and publishes it to AWS IoT.
    
    Expected ESP32 message format (adjust parsing below if yours differs):
        e.g.  "steps:42"  or  "42"  or  "steps:42,cadence:1.2"
    """
    global sequence
    raw = data.decode("utf-8").strip()
    print(f"\r📡 BLE raw: {raw}", end="", flush=True)

    # ── Parse the ESP32 message ──────────────────────────────────────────
    # Handles two common formats:
    #   1.  "key:value,key:value"  →  parsed into a dict
    #   2.  plain number string    →  stored as {"value": <number>}
    parsed = {}
    if ":" in raw:
        for pair in raw.split(","):
            if ":" in pair:
                k, v = pair.split(":", 1)
                k = k.strip()
                v = v.strip()
                try:
                    parsed[k] = float(v) if "." in v else int(v)
                except ValueError:
                    parsed[k] = v
    else:
        try:
            parsed["value"] = float(raw) if "." in raw else int(raw)
        except ValueError:
            parsed["raw"] = raw          # fallback: keep as plain string

    # ── Build MQTT payload ───────────────────────────────────────────────
    payload = {
        "sequence":  sequence,
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "device":    DEVICE_NAME,
        **parsed,                        # spread the parsed BLE data in
    }
    message_json = json.dumps(payload)

    # ── Publish ──────────────────────────────────────────────────────────
    try:
        mqtt_client.publish(TOPIC, message_json, 1)
        log.info(f"Published → {TOPIC}: {message_json}")
        sequence += 1
    except Exception as e:
        log.error(f"MQTT publish failed: {e}")

# ─────────────────────────────────────────────
# MAIN ASYNC LOOP
# ─────────────────────────────────────────────
async def main():
    # 1. Connect to AWS IoT first
    log.info("Connecting to AWS IoT Core...")
    mqtt_client.connect()
    log.info("AWS IoT connected.")

    # 2. Scan for the ESP32
    log.info(f"Scanning for '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=15)

    if device is None:
        log.error(f"Could not find '{DEVICE_NAME}'. Is the ESP32 powered on?")
        mqtt_client.disconnect()
        return

    log.info(f"Found device: {device.address}")
    log.info("Connecting over BLE...")

    async with BleakClient(device) as ble_client:
        log.info("BLE connected! Streaming data to AWS IoT...\n" + "-" * 40)

        await ble_client.start_notify(TX_UUID, handle_data)

        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\n")
            log.info("Stopped by user.")
            await ble_client.stop_notify(TX_UUID)

    mqtt_client.disconnect()
    log.info("AWS IoT disconnected.")

# ─────────────────────────────────────────────
# ENTRY POINT
# ─────────────────────────────────────────────
if __name__ == "__main__":
    asyncio.run(main())
