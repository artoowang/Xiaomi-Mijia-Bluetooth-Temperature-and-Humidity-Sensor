#include <string>

class BleScan {
 public:
  BleScan(std::string bluetooth_addr);

 private:
  std::string bluetooth_addr_;
};
