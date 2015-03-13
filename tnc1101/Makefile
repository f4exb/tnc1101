EXTRA_CFLAGS := -DMAX_VERBOSE_LEVEL=4

all: tnc1101 

clean:
	rm -f *.o tnc1101
	 

tnc1101: main.o usbserial.o util.o usb_test.o serial.o radio.o
	$(CCPREFIX)gcc $(LDFLAGS) -s -lm -o tnc1101 main.o usbserial.o serial.o util.o usb_test.o radio.o

main.o: main.h main.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o main.o main.c

radio.o: main.h radio.h radio.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o radio.o radio.c

serial.o: main.h serial.h serial.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o serial.o serial.c

usb_test.o: usb_test.h usb_test.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o usb_test.o usb_test.c

util.o: util.h util.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o util.o util.c
