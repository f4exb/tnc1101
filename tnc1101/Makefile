EXTRA_CFLAGS := -I../common -DMAX_VERBOSE_LEVEL=4

all: tnc1101 

clean:
	rm -f *.o tnc1101
	 

tnc1101: main.o util.o usb_test.o serial.o radio.o test.o bulk.o
	$(CCPREFIX)gcc $(LDFLAGS) -s -lm -o tnc1101 main.o serial.o util.o usb_test.o test.o radio.o bulk.o

main.o: ../common/msp430_interface.h main.h main.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o main.o main.c

radio.o: ../common/msp430_interface.h main.h radio.h radio.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o radio.o radio.c

serial.o: main.h serial.h serial.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o serial.o serial.c

usb_test.o: ../common/msp430_interface.h usb_test.h usb_test.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o usb_test.o usb_test.c

test.o: ../common/msp430_interface.h test.h radio.h main.h test.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o test.o test.c

bulk.o: ../common/msp430_interface.h bulk.h radio.h main.h bulk.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o bulk.o bulk.c

util.o: util.h util.c
	$(CCPREFIX)gcc $(CFLAGS) $(EXTRA_CFLAGS) -c -o util.o util.c
