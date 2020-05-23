#include "ble_scan.h"

#include <iostream>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace {

// Don't filter duplicates. On Rpi, if I filter duplicates I'll only get the
// first message.
constexpr uint8_t kFilterDuplicate = 0x00;
// Passive (0x01 = normal scan).
constexpr uint8_t kScanType = 0x00;
constexpr uint8_t kOwnType = LE_PUBLIC_ADDRESS;
// Whitelist (0x00 = normal scan).
constexpr uint8_t kFilterPolicy = 0x01;

}  // namespace

BleScan::BleScan() {}

BleScan::~BleScan() {
  if (dd_ < 0) {
    return;
  }
  // These need to be called at all times, or otherwise bluetooth device will be
  // locked and needs to be restarted.
  if (hci_le_set_scan_enable(dd_, /*enable=*/0x00, kFilterDuplicate,
                             /*to=*/10000) < 0) {
    perror("hci_le_set_scan_enable failed");
  }
  if (hci_close_dev(dd_) < 0) {
    perror("hci_close_dev failed");
  }
  printf("BleScan destroyed.\n");  // TODO
}

// TODO: support multiple devices.
bool BleScan::Initialize(std::string bluetooth_addr) {
  int dev_id = hci_get_route(nullptr);
  if (dev_id < 0) {
    perror("hci_get_route failed");
    return false;
  }
  dd_ = hci_open_dev(dev_id);
  if (dd_ < 0) {
    perror("hci_open_dev failed");
    return false;
  }
  // Clear white list.
  if (hci_le_clear_white_list(dd_, /*to=*/1000) < 0) {
    perror("hci_le_clear_white_list failed");
    return false;
  }
  // Add BT devices to white list.
  bdaddr_t device_addr;
  if (str2ba(bluetooth_addr.c_str(), &device_addr) < 0) {
    std::cerr << "Bad BT address: " << bluetooth_addr << std::endl;
    return false;
  }
  if (hci_le_add_white_list(dd_, &device_addr, kOwnType, /*to=*/1000) <
      0) {
    perror("hci_le_add_white_list failed");
    return false;
  }
  // Start scan.
  const uint16_t interval = htobs(0x0010);
  const uint16_t window = htobs(0x0010);
  if (hci_le_set_scan_parameters(dd_, kScanType, interval, window, kOwnType,
                                 kFilterPolicy, /*to=*/10000) < 0) {
    perror("hci_le_set_scan_parameters failed");
    return false;
  }
  if (hci_le_set_scan_enable(dd_, /*enable=*/0x01, kFilterDuplicate,
                             /*to=*/10000) < 0) {
    perror("hci_le_set_scan_enable failed");
    return false;
  }
  return true;
}
