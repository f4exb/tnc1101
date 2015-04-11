  tnc1101
=======

Semi-virtual TNC using MSP430 + CC1101 RF module to send data blocks over the air and a host client application to handle AX.25/KISS protocol

#Introduction

It assumes the following architecture is in place:
 <pre><code>
+--------------------------------------+
| Host         Rasp-Pi, whatever...    |
|                                      |
|         TCP/IP                       |
|           |                          |
|  +-----------------------+           |
|  | ax0 network interface |           |
|  +-----------------------+           |
|           ^                          |
|           | AX.25/KISS               |
|           V                          |
|     /var/ax25/axp1                   |
|           |                          |
|           | Virtual serial cable     | 
|           |                          |
|     /var/ax25/axp2                   |
|           ^                          |
|           |                          |
|           V                          |
|   +--------------------+             |
|   | tnc1101 vitual TNC |<- User parameters
|   +--------------------+             |
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

The application has two main components
  - The MCU (MSP430F5529 Launchpad board) interface with the CC1101 RF module in `msp430` folder. It comprises the follwing functions:
    - Initialize the CC1101 chip with parameters specified by the host client application
    - Handle the CC1101 Rx and Tx 64 bytes FIFOs to handle data blocks up to 255 bytes called "radio blocks"
  - The client application on the host that communicate with the MCU over USB. Located in `tnc1101` folder it does the following:
    - Instructs the MCU to initialize the CC1101 module with the desired parameters
    - Interface with the TNC end of the virtual serial link cable. See "AX.25/KISS operation" chapter about details on this virtual "cable".
    - Handle KISS blocks and split them over radio blocks of up to 255 bytes
    - Move radio blocks back and forth to the MCU via USB
    - Provides diagnostics

Additional folders:
  - The USB stack and interface to other functionnalities of the MSP430F5529 Launchpad board (SPI, ...) from TI are located in `MSP430_USB_API` folder.
  - The interface with the CC1101 module is located in `MSP430_cc1101`
  - Common files interfacing host and MCU software are located in the `common` folder
  - The `scripts` directory contains convenience shell scripts to set-up and bring down AX25/KISS or SLIP interfaces
