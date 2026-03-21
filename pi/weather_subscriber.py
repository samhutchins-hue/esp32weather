#!/usr/bin/env python3

import json
import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient
from influxdb_client import Point
from influxdb_client.client.write_api import SYNCHRONOUS
from influxdb_client.client.exceptions import InfluxDBError
from dotenv import load_dotenv
import os

load_dotenv()

MQTT_BROKER = os.getenv("MQTT_BROKER")
MQTT_TOPIC = os.getenv("MQTT_TOPIC")

INFLUX_BUCKET = os.getenv("MQTT_BUCKET")
INFLUX_ORG = os.getenv("INFLUX_ORG")
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN")
INFLUX_URL= os.getenv("INFLUX_URL")

# create the client
client = InfluxDBClient(
    url=INFLUX_URL,
    token=INFLUX_TOKEN,
    org=INFLUX_ORG
)

# set write client
write_api = client.write_api(write_options=SYNCHRONOUS)

# create a test point object
# and write to client with (bucket, org, record)

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    mqttc.subscribe(MQTT_TOPIC)

def on_message(mqttc, obj, msg):
    try:
        result = json.loads(msg.payload)
        print(result)
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON: {e.msg}")
        print(f"Error location: Line {e.lineno}, Column {e.colno}")
        return

    temperature = result['temperature']
    pressure = result['pressure']
    timestamp = result['timestamp']

    p = Point("my_measurement").tag("timestamp", timestamp).field("temperature", temperature).field("pressure", pressure)

    try:
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=p)
    except InfluxDBError as e:
        print(f"InfluxDB error: {e}")
        return

def on_subscribe(mqttc, obj, mid, reason_code_list, properties):
    print("Subscribed: " + str(mid) + " " + str(reason_code_list))


def on_log(mqttc, obj, level, string):
    print(string)


# If you want to use a specific client id, use
# mqttc = mqtt.Client("client-id")
# but note that the client id must be unique on the broker. Leaving the client
# id parameter empty will generate a random id for you.
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqttc.on_subscribe = on_subscribe
# Uncomment to enable debug messages
# mqttc.on_log = on_log

mqttc.connect(MQTT_BROKER, 1883, 60)

mqttc.loop_forever()
