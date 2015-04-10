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


