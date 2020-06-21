#!/usr/bin/env python3

from BleScan import BleScan
from mijia_config import mijia_config

import paho.mqtt.client as mqtt
import signal
import sys

def signal_handler(sig, frame):
  # Do nothing. This is just to keep Python going. The BleScan#read below should
  # return None.
  pass

def ExitWithError(*args, **kwargs):
  print(*args, file=sys.stderr, **kwargs)
  sys.exit(1)

def decodeTemperatureFahrenheit(upper_byte, lower_byte):
  return (upper_byte << 8 | lower_byte) * 0.1 * 9.0 / 5.0 + 32.0

def decodeRelativeHumidity(upper_byte, lower_byte):
  return (upper_byte << 8 | lower_byte) * 0.1

def decodeIlluminance(upper_byte, mid_byte, lower_byte):
  return upper_byte << 16 | mid_byte << 8 | lower_byte

def decodeConductivity(upper_byte, lower_byte):
  return upper_byte << 8 | lower_byte

def onMessage(client, userdata, message):
  topic = str(message.topic)
  message = str(message.payload.decode("utf-8"))
  print(topic + message)

def onConnect(client, userdata, flags, rc):
  print("Connected with result code " + str(rc))
  if rc != 0:
    sys.exit(1)

def data2str(data):
  data_str = ""
  for c in data:
    data_str += "%02x " % c
  return data_str

def publish(client, topic, value_str):
  print("%s: %s" % (topic, value_str))
  client.publish(topic, "{\"v\": %s}" % value_str)

if len(sys.argv) < 4:
  sys.stderr.write('Usage: %s <mqtt_username> <mqtt_password> <mqtt_ip> ' %
                   sys.argv[0])
  sys.exit(1)

signal.signal(signal.SIGINT, signal_handler)

mqtt_username = sys.argv[1]
mqtt_password = sys.argv[2]
mqtt_ip = sys.argv[3]

print("Configurations:\n" + str(mijia_config))

bt_addrs = list(mijia_config.keys())

ble_scan = BleScan()
if not ble_scan.initialize(bt_addrs):
  ExitWithError("Failed to initialize BleScan.")

client = mqtt.Client()
client.username_pw_set(mqtt_username, mqtt_password)
client.on_connect = onConnect
client.on_message = onMessage
client.connect(mqtt_ip, 1883)
client.loop_start()

while True:
  result = ble_scan.read()
  # Ctrl-C triggers this.
  if result is None:
    break
  (addr, data) = result

  # Find where the message is. This is to mimic what 
  # https://github.com/esphome/esphome/blob/dev/esphome/components/xiaomi_ble/xiaomi_ble.cpp
  # is doing. Somehow the esp32_ble_tracker::ServiceData seems to be able to
  # point to the correct place in the data, but we don't have that here.
  # Instead, we check 2 known positions in the returned data for 0x95 and 0xFE
  # magic bytes, and choose the latter one.
  known_magic_bytes_positions = [9, 5]
  raw = None
  for pos in known_magic_bytes_positions:
    if data[pos] == 0x95 and data[pos+1] == 0xFE:
      raw = data[pos+2:]
      break
  if raw is None:
    print("Cannot find magic bytes: " + data2str(data))
    continue

  device_type = None
  if raw[2] == 0x98 and raw[3] == 0x00:
    device_type = "HHCCJCY01"
  elif raw[2] == 0x47 and raw[3] == 0x03:
    device_type = "CGG1"
  elif raw[2] == 0xaa and raw[3] == 0x01:
    device_type = "LYWSDCGQ"

  if device_type is None:
    print("Unrecognized device type: %02x %02x" % (raw[2], raw[3]))
    continue

  # Copied from xiaomi_ble.cpp.
  has_capability = raw[0] & 0x20 != 0
  if has_capability:
    raw_offset = 12
  else:
    raw_offset = 11

  msg = raw[raw_offset:]
  if len(msg) < 3:
    print("Message too short: " + data2str(msg))
    continue
  msg_length = msg[2]
  if msg[1] != 0x10 or msg_length != len(msg) - 3:
    print("Unrecognized message: " + data2str(msg))
    continue

  result = {}
  if msg[0] == 0x0D and msg_length == 4:  # Temperature and humidity.
    result['temperature_fahrenheit'] = decodeTemperatureFahrenheit(
        msg[4], msg[3])
    result['relative_humidity'] = decodeRelativeHumidity(msg[6], msg[5])
  elif msg[0] == 0x0A and msg_length == 1:  # Battery.
    result['battery_percentage'] = msg[3]
  elif msg[0] == 0x04 and msg_length == 2:  # Temperature.
    result['temperature_fahrenheit'] = decodeTemperatureFahrenheit(
       msg[4], msg[3])
  elif msg[0] == 0x06 and msg_length == 2:  # Humidity.
    result['relative_humidity'] = decodeRelativeHumidity(msg[4], msg[3])
  elif msg[0] == 0x07 and msg_length == 3:  # Illuminance.
    result['illuminance'] = decodeIlluminance(msg[5], msg[4], msg[3])
  elif msg[0] == 0x08 and msg_length == 1:  # Soil moisture.
    result['soil_moisture_percentage'] = msg[3]
  elif msg[0] == 0x09 and msg_length == 2:  # Conductivity.
    result['conductivity'] = decodeConductivity(msg[4], msg[3])
  else:
    print("Unrecognized message type: " + data2str(msg))
    continue

  if addr not in mijia_config:
    print("Unexpected MAC: %s" % addr)
    continue
  msg_topic_map = mijia_config[addr]
  
  for msg_name in msg_topic_map:
    topic = msg_topic_map[msg_name]
    if msg_name in result:
      publish(client, topic, result[msg_name])

# TODO(artoowang): Should consider using "with" statement:
# https://stackoverflow.com/questions/6772481/how-to-force-deletion-of-a-python-object
del ble_scan
print("Shutting down ...")
