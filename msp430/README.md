  tnc1101/msp430
=======

Semi-virtual TNC using MSP430 + CC1101 RF module to send data blocks over the air and a host client application to handle AX.25/KISS protocol

#Introduction

This is the source of the microcode for the MSP430F5529. This is the part interfacing the host via USB CDC and the CC1101 RF module via SPI

 <pre><code>

Host...

|           ^                          |
+-----------|--------------------------+
            | Serial over USB
+-----------|--------------------------+
| MCU       |             MSP430F5529  |
|           v             Launchpad    |
|   +------------------------+         |
|   | CC1101 control program |         |
|   +------------------------+         |
|           ^                          |
+-----------|--------------------------+
            | SPI
            |                   \\|/
            V                    |
    +-----------------+          |
    | CC1101 module   |----------+
    +-----------------+
</code></pre>

#Operating the CC1101 RF interface

For details on the CC1101 module please refer to [TI's documentation](http://www.ti.com/product/cc1101)

The global structure of the code is as follows:
  - process I/O with the host via USB in the main infinite loop
  - handle the CC1101 FIFO via the GDO0 and GDO2 interrupt handlers 


#Connecting the CC1101 module
In most cases the 8-pin head layout on the CC1101 is the following seen from the pin side:
 <pre><code>
    +-----+
MISO|.7 8.|GDO2
 SCK|.5 6.|MOSI
GDO0|.3 4.|CSn
 GND|.1 2.|VCC
    +-----+
</code></pre>

#Connecting the MSP430F5529 Launchpad

On the Launchpad side connections are the following:

<pre><code>
VCC--+3.3V       +5V                   P2.5        GND
      P6.5  GND--GND                   P2.4        P2.0
      P3.4       P6.0                  P1.5--GDO0  P2.2--CSn
      P3.3       P6.1    +--------+    P1.4--GDO2  P7.4
      P1.6       P6.2    | MSP430 |    P1.3        RST
      P6.6       P6.3    |        |    P1.2        P3.0--MOSI
SCK---P3.2       P6.4    +--------+    P4.3        P3.1--MISO
      P2.7       P7.0                  P4.0        P2.6
      P4.2       P3.6                  P3.7        P2.3
      P4.1       P3.5                  P8.2        P8.1
</code></pre>


#Compile the source and load the code

## Install gcc compiler for MSP430

### Get the compiler

You will have to use the gcc compiler provided by TI and Red Hat. The Debian pacakge for example is too old. You can check TI's website page on the [GCC - Open Source Compiler for MSP430 Microcontrollers](http://www.ti.com/tool/msp430-gcc-opensource). Click on "Get Software" button to be taken to the donloads page where you can choose to download the file corresponding to your installation. [Direct link](http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSPGCC/latest/index_FDS.html)

### Compile from source

If you choose to build from source you need to download the tar.bz2 archive from the page mentioned before: [Direct link](http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSPGCC/latest/exports/msp430-gcc-source.tar.bz2) 

Install the source in some directory on your machine for example /opt/build
  - `cd /opt/build`
  - `sudo apt-get install bzip2` (bzip2 compression is not installed by default. You make skip this if bzip2 is already installed)
  - `tar -xf msp430-gcc-source.tar.bz2`
  - `mv source msp430-elf-gcc` (more descriptive name for the source folder)

Install required packages:
  - libusb-dev
  - g++
  - bison
  - flex
  - libx11-dev
  - expect
  - texinfo
  - libncurses5-dev
  - gcc-4.9

Which is done with the following command:
  - `sudo apt-get install libusb-dev g++ bison flex libx11-dev expect texinfo libncurses5-dev`

On x86_64 architectures you also need to load the i386 (32 bit) libc6 dev package:
  - `sudo apt-get install libc6-dev-i386`

On 32 bit systems you will have to install the native libc6 dev package:
  - `sudo apt-get install libc6-dev`


Compile the compiler for your host architecture:
  - `cd msp430-elf-gcc`
  - `mkdir build`
  - `cd build`
  - `../tools/configure --prefix=/opt/install/msp430-elf-gcc --target=msp430-elf --enable-languages=c,c++ --disable-itcl --disable-tk --disable-tcl --disable-libgui --disable-gdbtk` (this will install the compiler in /opt/install/msp430-elf-gcc. You may change the --prefix parameter to fit your needs)
  - `make -j<n>` (where <n> is the number of logical CPUs + 1. ex: on core i7 this is 9)
  - `make install`

Cross-compile (may not work):
  - `export PATH=<path to the bin directory of your cross-compiler>:$PATH`
  - `./tools/configure --host=arm-cortexa7_neonvfpv4-linux-gnueabihf --prefix=/opt/install/armv7/msp430-elf-gcc --target=msp430-elf --enable-languages=c,c++ --disable-itcl --disable-tk --disable-tcl --disable-libgui --disable-gdbtk` (example for Cortex A7)

## Compile the microcode source

You need to set the `REDHAT_GCC` variable in the Makefile to the installation path of the gcc compiler. For example:
  - `REDHAT_GCC = /opt/install/msp430-elf-gcc`

Then just run make. This produces a `msp430_cc1101.out` file that you will load into the MSP430F5529 Launchpad.

## Build the debugger and loader

Obtain mspdebug by cloning the git repository:
  - `git clone git://git.code.sf.net/p/mspdebug/code mspdebug`

You will need libusb and libreadline development packages:
  - `sudo apt-get install libusb-dev libreadline-dev`

Then build and install:
  - `make -j<n>` (<n> is the number of logical CPUs + 1)
  - `sudo make install` (or use the `mspdebug` executable in the same directory)

The only working option is to use the tilib driver and this needs a libmsp430.so library in /usr/lib. So you will need one for your host architecture.

## Build and install libmsp430.so

Download the source in the slac460.zip [from this page](http://processors.wiki.ti.com/index.php/MSPDS_Open_Source_Package). Direct link: [slac460.zip](http://www-s.ti.com/sc/techzip/slac460.zip)

Unzip somewhere and cd into the `MSPDebugStack_OS_Package` directory

You need to apply a patch that can be found [here](http://mspdebug.sourceforge.net/tilib.html#patch_460f). You can take the latest which is `slac460h_unix.diff` at the moment. The patch will work with some lines realignment. Use the patch command like this:
  - `patch -p1 < slac460h_unix.diff`

Install some pre-required packages:
  - `sudo apt-get install libhidapi-dev libboost-thread-dev libboost-filesystem-dev libboost-date-time-dev libboost-system-dev libusb-1.0-0-dev`

Run make as usual. You should obtain a `libmsp430.so` at the root of the directory. Then just copy it to `/usr/lib`
  - `sudo cp -v libmsp430.so /usr/lib`

## Upgrade the Launchpad firmware

It might be necessary to upgrade the Launchpad's firmware:
  - `sudo mspdebug tilib --allow-fw-update`

Note that sudo is important here. It will accept to run without it but the result is a broken Launchpad that does not enumerate as 2047:0013 as it should. It is 2047:0203 instead which is typical of this situation.

To recover you have to run the fw update TWICE as sudo
  - `sudo mspdebug tilib --allow-fw-update` (it will complain. unplug and replug...)
  - `sudo mspdebug tilib --allow-fw-update` (now it should work)

## Load the microcode into the MSP430F5529 Launchpad

Load the code:
  - `sudo mspdebug tilib 'erase' 'load msp430_cc1101.out' 'exit'`