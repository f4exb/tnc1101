EXTRA_CFLAGS := -DMAX_VERBOSE_LEVEL=4

all: tnc1101 

clean:
	rm -f *.o tnc1101
	 

tnc1101: main.o usbserial.o util.o usb_test.o serial.o
	$(CCPREFIX)gcc $(LDFLAGS) -s -lm -o tnc1101 main.o usbserial.o serial.o util.o usb_test.o

main.o: main.h main.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o main.o main.c

serial.o: main.h serial.h serial.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o serial.o serial.c

usbserial.o: main.h usbserial.h usbserial.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o usbserial.o usbserial.c

usb_test.o: usb_test.h usb_test.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o usb_test.o usb_test.c

util.o: util.h util.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o util.o util.c
