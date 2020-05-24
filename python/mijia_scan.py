#!/usr/bin/env python3

from BleScan import BleScan

import sys

def ExitWithError(*args, **kwargs):
  print(*args, file=sys.stderr, **kwargs)
  sys.exit(1)

def GetUInt16(upper_byte, lower_byte):
  return upper_byte << 8 | lower_byte

if len(sys.argv) < 2:
  sys.stderr.write('Usage: %s <Bluetooth_address>\n' % sys.argv[0])
  sys.exit(1)

b = BleScan()
if not b.initialize(sys.argv[1]):
  ExitWithError("Failed to initialize BleScan.")

while True:
  data = b.read()
  #print(len(data))  # TODO
  #print(data)  # TODO
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
    print("T %f" % temperature_fahrenheit)
  if has_humidity:
    print("H %f" % relative_humidity)
  if has_battery:
    print("B %d" % battery_percentage)
  #import pdb; pdb.set_trace()
