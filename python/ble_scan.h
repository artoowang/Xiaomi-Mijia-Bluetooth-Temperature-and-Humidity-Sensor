#include <optional>
#include <string>
#include <vector>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <boost/python.hpp>

class BleScan {
 public:
  BleScan();
  ~BleScan();

  bool Initialize(const boost::python::list& bluetooth_addrs);
  PyObject* Read();

 private:
  std::vector<bdaddr_t> bluetooth_addrs_;
  int dd_ = -1;
  std::optional<struct hci_filter> previous_filter_;
};
