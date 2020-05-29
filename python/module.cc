#include <boost/python.hpp>

#include "ble_scan.h"

using namespace boost::python;

BOOST_PYTHON_MODULE(BleScan) {
  class_<StrList>("StrList").def(vector_indexing_suite<StrList>());

  class_<BleScan>("BleScan", init<>())
      .def("initialize", &BleScan::Initialize)
      .def("read", &BleScan::Read);
}
