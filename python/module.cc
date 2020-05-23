#include <boost/python.hpp>

#include "ble_scan.h"

using namespace boost::python;

BOOST_PYTHON_MODULE(BleScan) {
  class_<BleScan>("BleScan", init<std::string>());
}
