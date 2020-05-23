#include <optional>
#include <string>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

class BleScan {
 public:
  BleScan();
  ~BleScan();

  bool Initialize(std::string bluetooth_addr);
  std::string Read();

 private:
  std::string bluetooth_addr_;
  int dd_ = -1;
  std::optional<struct hci_filter> previous_filter_;
};
