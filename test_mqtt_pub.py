"""
Script test: Publish message toi broker MQTT de test ESP32 subscribe/listen.
Broker: 103.75.187.153:1885
Topic:  GC1765606666
"""
import paho.mqtt.client as mqtt
import sys
import time

BROKER   = "103.75.187.153"
PORT     = 1885
TOPIC    = "GC1765606666"
CLIENT_ID = "test_publisher_pc"

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"[OK] Connected to {BROKER}:{PORT}")
    else:
        print(f"[FAIL] Connect failed, rc={rc}")
        sys.exit(1)

def on_publish(client, userdata, mid, *args):
    print(f"[OK] Message published (mid={mid})")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID)
client.on_connect = on_connect
client.on_publish = on_publish

print(f"Connecting to {BROKER}:{PORT}...")
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()
time.sleep(2)  # wait for connection

# Message to send
default_msg = '{"postCode":"GC59286691881","state":2,"paymentDriverCode":"GC8PHV3MEQ","maxAmount":14877,"sessionCode":1775403288501,"pricePost":5000}'
message = sys.argv[1] if len(sys.argv) > 1 else default_msg
print(f"Publishing to [{TOPIC}]: {message}")
result = client.publish(TOPIC, message, qos=1)
result.wait_for_publish()

time.sleep(1)
client.disconnect()
client.loop_stop()
print("Done.")
