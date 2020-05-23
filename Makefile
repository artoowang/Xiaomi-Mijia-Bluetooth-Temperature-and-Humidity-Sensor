scanMijia:
	g++ -O2 -o $@ scanMijia.cc -lbluetooth

clean:
	rm -f *.o *~ core scanMijia 
