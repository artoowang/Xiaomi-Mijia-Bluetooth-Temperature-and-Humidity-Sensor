#!/usr/bin/env python3

from BleScan import BleScan

import paho.mqtt.client as mqtt
import sys

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

if len(sys.argv) < 8:
  # TODO: uncomment after arguments are decided.
  #sys.stderr.write('Usage: %s <Bluetooth_address>\n' % sys.argv[0])
  sys.exit(1)

bt_address = sys.argv[1]
mqtt_username = sys.argv[2]
mqtt_password = sys.argv[3]
mqtt_ip = sys.argv[4]
topic_temp = sys.argv[5]
topic_humid = sys.argv[6]
topic_battery = sys.argv[7]

ble_scan = BleScan()
if not ble_scan.initialize(bt_address):
  ExitWithError("Failed to initialize BleScan.")

client = mqtt.Client()
client.username_pw_set(mqtt_username, mqtt_password)
client.on_connect = onConnect
client.on_message = onMessage
client.connect(mqtt_ip, 1883)
client.loop_start()

while True:
  data = ble_scan.read()
  if len(data) != 22 and len(data) != 23 and len(data) != 25:
    continue
  if data[4] != 0x16 or data[5] != 0x95 or data[6] != 0xFE:
    continue

  has_temperature = False
  has_humidity = False
  has_battery = False
  if data[18] == 0x0D:  # Temperature and humidity.
    temperature_celsius = GetUInt16(data[22], data[21]) * 0.1
    has_temperature = True
    relative_humidity = GetUInt16(data[24], data[23]) * 0.1
    has_humidity = True
  elif data[18] == 0x0A:  # Battery.
    battery_percentage = data[21]
    has_battery = True
  elif data[18] == 0x04:  # Temperature.
    temperature_celsius = GetUInt16(data[22], data[21]) * 0.1
    has_temperature = True
  elif data[18] == 0x06:  # Humidity.
    relative_humidity = GetUInt16(data[22], data[21]) * 0.1
    has_humidity = True

  if has_temperature:
    temperature_fahrenheit = temperature_celsius * 9.0 / 5.0 + 32.0
    print("T %f" % temperature_fahrenheit)  # TODO
    client.publish(topic_temp, "{\"temperatureF\": %f}" %
                   temperature_fahrenheit)
  if has_humidity:
    print("H %f" % relative_humidity)  # TODO
    client.publish(topic_humid, "{\"humidity\": %f}" % relative_humidity)
  if has_battery:
    print("B %d" % battery_percentage)  # TODO
    client.publish(topic_battery, "{\"battery\": %d}" % battery_percentage)
