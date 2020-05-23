BleScan:
	g++ -shared -fPIC -Wl,--no-undefined -I/usr/include/python3.6m ble_scan.cc ble_scan.h module.cc -o BleScan.so -lboost_python3 -lboost_thread -lpython3.6m -lbluetooth

clean:
	rm -f *.o *~ core BleScan.so
