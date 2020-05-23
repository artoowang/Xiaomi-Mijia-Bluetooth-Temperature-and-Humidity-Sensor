PYTHON_CONFIG = python3-config
PYTHON_INCLUDES = $$($(PYTHON_CONFIG) --includes)
PYTHON_LIBS = $$($(PYTHON_CONFIG) --libs)

BleScan:
	g++ -shared -fPIC -Wl,--no-undefined $(PYTHON_INCLUDES) ble_scan.cc ble_scan.h module.cc -o BleScan.so -lboost_python3 -lboost_thread $(PYTHON_LIBS) -lbluetooth

clean:
	rm -f *.o *~ core BleScan.so
