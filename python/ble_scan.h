#include <string>

class BleScan {
 public:
  BleScan();
  ~BleScan();

  bool Initialize(std::string bluetooth_addr);

 private:
  std::string bluetooth_addr_;
  int dd_ = -1;
};
