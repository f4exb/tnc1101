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
            |                   \|/
            V                    |
    +-----------------+          |
    | CC1101 module   |----------+
    +-----------------+
</code></pre>

#Operating the CC1101 RF interface

For details on the CC1101 module please refer to (TI's documentation)[http://www.ti.com/product/cc1101]

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



</code></pre>


