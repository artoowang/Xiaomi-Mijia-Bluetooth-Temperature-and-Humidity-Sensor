#include "ble_scan.h"

#include <iostream>

#include <bluetooth/hci_lib.h>
#include <unistd.h>

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
  if (previous_filter_.has_value()) {
    // Restore previous socket settings.
    if (setsockopt(dd_, SOL_HCI, HCI_FILTER, &*previous_filter_,
                   sizeof(*previous_filter_)) < 0) {
      std::cerr << "Cannot restore previous socket settings." << std::endl;
    }
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
  if (str2ba(bluetooth_addr.c_str(), &bluetooth_addr_) < 0) {
    std::cerr << "Bad BT address: " << bluetooth_addr << std::endl;
    return false;
  }

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
  if (hci_le_add_white_list(dd_, &bluetooth_addr_, kOwnType, /*to=*/1000) < 0) {
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
  // Backup previous HCI filter settings.
  struct hci_filter filter;
  socklen_t olen = sizeof(filter);
  if (getsockopt(dd_, SOL_HCI, HCI_FILTER, &filter, &olen) < 0) {
    std::cerr << "Could not get socket options." << std::endl;
    return false;
  }
  previous_filter_ = filter;
  // Set desired filter settings.
  hci_filter_clear(&filter);
  hci_filter_set_ptype(HCI_EVENT_PKT, &filter);
  hci_filter_set_event(EVT_LE_META_EVENT, &filter);
  if (setsockopt(dd_, SOL_HCI, HCI_FILTER, &filter, sizeof(filter)) < 0) {
    std::cerr << "Could not set socket options." << std::endl;
    return false;
  }
  return true;
}

// TODO: Support returning address and timeout.
PyObject* BleScan::Read() {
  if (dd_ < 0) {
    std::cerr << "Cannot read: BleScan is not initialized." << std::endl;
    return nullptr;
  }
  char buf[HCI_MAX_EVENT_SIZE];
  int len = read(dd_, buf, sizeof(buf));
  if (len < 0) {
    perror("Failed to read");
    return nullptr;
  }

  if (len <= HCI_EVENT_HDR_SIZE + 1) {
    std::cerr << "Received data is too small. Expected more then "
              << HCI_EVENT_HDR_SIZE + 1 << " bytes, but received only " << len;
    return nullptr;
  }
  const auto *meta = reinterpret_cast<const evt_le_meta_event *>(
      buf + (1 + HCI_EVENT_HDR_SIZE));
  if (meta->subevent != 0x02) {
    std::cerr << "Unexpected evt_le_meta_event::subevent: expected 0x02, but "
                 "received "
              << meta->subevent;
    return nullptr;
  }

  // Currently ignore multiple reports, and only process the first one.
  const auto *info =
      reinterpret_cast<const le_advertising_info *>(meta->data + 1);

  // Sanity check to see if the received report comes from expected source.
  if (memcmp(&bluetooth_addr_, &info->bdaddr, sizeof(bdaddr_t) != 0)) {
    char expected[18] = {0};
    char received[18] = {0};
    ba2str(&bluetooth_addr_, expected);
    ba2str(&info->bdaddr, received);
    std::cerr << "Unexpected event from Bluetooth address " << received
              << ". Was expecting from " << expected;
  }

  return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(info->data),
                                   info->length);
}
