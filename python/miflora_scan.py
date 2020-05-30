#!/usr/bin/env python3

from BleScan import BleScan

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

def GetUInt16(upper_byte, lower_byte):
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

if len(sys.argv) < 8:
  sys.stderr.write('Usage: %s <mqtt_username> <mqtt_password> <mqtt_ip> '
                   '<topic_temp> <topic_humid> <topic_battery> '
                   '<bluetooth_addr1> [<bluetooth_addre2> ...]\n' % sys.argv[0])
  sys.exit(1)

signal.signal(signal.SIGINT, signal_handler)

mqtt_username = sys.argv[1]
mqtt_password = sys.argv[2]
mqtt_ip = sys.argv[3]
topic_temp = sys.argv[4]
topic_humid = sys.argv[5]
topic_battery = sys.argv[6]
bt_addrs = sys.argv[7:]

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

  print(data2str(msg))

  #has_temperature = False
  #has_humidity = False
  #has_battery = False
  #if data[18] == 0x0D:  # Temperature and humidity.
  #  temperature_celsius = GetUInt16(data[22], data[21]) * 0.1
  #  has_temperature = True
  #  relative_humidity = GetUInt16(data[24], data[23]) * 0.1
  #  has_humidity = True
  #elif data[18] == 0x0A:  # Battery.
  #  battery_percentage = data[21]
  #  has_battery = True
  #elif data[18] == 0x04:  # Temperature.
  #  temperature_celsius = GetUInt16(data[22], data[21]) * 0.1
  #  has_temperature = True
  #elif data[18] == 0x06:  # Humidity.
  #  relative_humidity = GetUInt16(data[22], data[21]) * 0.1
  #  has_humidity = True

  #if has_temperature:
  #  temperature_fahrenheit = temperature_celsius * 9.0 / 5.0 + 32.0
  #  print("T %f" % temperature_fahrenheit)  # TODO
  #  client.publish(topic_temp, "{\"temperatureF\": %f}" %
  #                 temperature_fahrenheit)
  #if has_humidity:
  #  print("H %f" % relative_humidity)  # TODO
  #  client.publish(topic_humid, "{\"humidity\": %f}" % relative_humidity)
  #if has_battery:
  #  print("B %d" % battery_percentage)  # TODO
  #  client.publish(topic_battery, "{\"battery\": %d}" % battery_percentage)

# TODO(artoowang): Should consider using "with" statement:
# https://stackoverflow.com/questions/6772481/how-to-force-deletion-of-a-python-object
del ble_scan
print("Shutting down ...")
