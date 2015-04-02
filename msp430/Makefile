DEVICE = MSP430F5529
MCU=msp430f5529
PROG_NAME = msp430_cc1101
USB_BASE = ../MSP430_USB_API
REDHAT_GCC = /opt/install/msp430-elf-gcc
GCC_BIN = $(REDHAT_GCC)/bin
CC = $(GCC_BIN)/msp430-elf-gcc
GDB = $(GCC_BIN)/msp430-elf-gdb
SUPPORT_FILE_DIRECTORY = $(REDHAT_GCC)/msp430-gcc-support-files
DRIVERLIB_DIR = $(USB_BASE)/driverlib/MSP430F5xx_6xx
MSP430_FILE = $(USB_BASE)/USB_API/msp430USB.ld
PROJECT_ROOT = .
USB_CONFIG = $(PROJECT_ROOT)/USB_config
USB_API = $(USB_BASE)/USB_API


CFLAGS = -I../common -I $(SUPPORT_FILE_DIRECTORY) -I $(USB_BASE) -I $(DRIVERLIB_DIR) -I $(PROJECT_ROOT) -I $(USB_CONFIG) -I $(USB_API) -D__$(DEVICE)__ -mmcu=$(MCU) -O3 -g -fdata-sections -w
LFLAGS = -T $(MSP430_FILE) -T $(SUPPORT_FILE_DIRECTORY)/msp430f5529.ld -Wl,--gc-sections

SRC_FILES = \
hal.c \
util.c \
main.c \
system_pre_init.c \
USB_app/usbConstructs.c \
USB_app/usbEventHandling.c \
USB_config/descriptors.c \
USB_config/UsbIsr.c \
../MSP430_USB_API/USB_API/USB_CDC_API/UsbCdc.c \
../MSP430_USB_API/USB_API/USB_Common/usb.c \
../MSP430_USB_API/USB_API/USB_Common/usbdma.c \
../MSP430_USB_API/USB_API/USB_HID_API/UsbHid.c \
../MSP430_USB_API/USB_API/USB_HID_API/UsbHidReq.c \
../MSP430_USB_API/USB_API/USB_MSC_API/UsbMscReq.c \
../MSP430_USB_API/USB_API/USB_MSC_API/UsbMscScsi.c \
../MSP430_USB_API/USB_API/USB_MSC_API/UsbMscStateMachine.c \
../MSP430_USB_API/USB_API/USB_PHDC_API/UsbPHDC.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/adc10_a.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/adc12_a.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/aes.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/battbak.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/comp_b.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/crc.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/dac12_a.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/dma.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_a_spi.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_a_uart.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_b_i2c.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_b_spi.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_i2c.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_spi.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/eusci_uart.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/flashctl.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/gpio.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/ldopwr.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/mpy32.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/pmap.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/pmm.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/ram.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/ref.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/rtc_a.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/rtc_b.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/rtc_c.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/sd24_b.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/sfr.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/sysctl.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/tec.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/timer_a.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/timer_b.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/timer_d.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/tlv.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/ucs.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_a_spi.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_a_uart.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_b_i2c.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_b_spi.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_i2c.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_spi.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/usci_uart.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/wdt_a.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/dma.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/flashctl.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/mpy32.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/pmm.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/ram.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/sfr.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/sysctl.c \
../MSP430_USB_API/driverlib/MSP430F5xx_6xx/deprecated/ucs.c \

$(PROG_NAME): $(SRC_FILES) TI_CC_spi.o radio.o 
	$(CC) $(CFLAGS) -I../MSP430_cc1101 $(LFLAGS) $? -o $(PROG_NAME).out

TI_CC_spi.o: ../MSP430_cc1101/TI_CC_spi.c ../MSP430_cc1101/TI_CC_spi.h ../MSP430_cc1101/TI_CC_msp430.h ../MSP430_cc1101/TI_CC_hardware_board.h ../MSP430_cc1101/TI_CC_CC1100-CC2500.h
	$(CC) $(CFLAGS) -I../MSP430_cc1101 -c -o TI_CC_spi.o ../MSP430_cc1101/TI_CC_spi.c

radio.o: radio.c radio.h
	$(CC) $(CFLAGS) -I../MSP430_cc1101 -c -o radio.o radio.c

debug: $(PROG_NAME)
	$(GDB) $(PROG_NAME).out

clean:
	rm -f *.o $(PROG_NAME).out