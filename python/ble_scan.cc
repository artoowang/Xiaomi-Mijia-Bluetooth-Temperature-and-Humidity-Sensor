#include "ble_scan.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

BleScan::BleScan() {}

BleScan::~BleScan() {
  if (dd_ >= 0) {
    if (hci_close_dev(dd_) < 0) {
      perror("hci_close_dev failed");
    }
  }
}

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
  return true;
}
