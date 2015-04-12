  tnc1101/tnc1101
=======

Semi-virtual TNC using MSP430 + CC1101 RF module to send data blocks over the air and a host client application to handle AX.25/KISS protocol

#Introduction

This is the code running on the host that communicates with the MSP430F5529 Launchpad via USB CDC.
 <pre><code>
+--------------------------------------+
| Host     Banana-Pi, whatever...      |
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

</code></pre>

# Installation and basic usage
## Prerequisites
This has been tested successfully on a Banana Pi revision 2 with kernel 4.0.0-rc6. The version 2 of Raspberry-Pi (4 core) has problems with the USB handling.

## Obtain the code
Just clone the main tnc1101 repository in a local folder of your choice on the host. Change to the `tnc1101/tnc1101` directory:
  - `git clone https://github.com/f4exb/tnc1101` 
  - `cd tnc1101/tnc1101`

## Compilation
You can compile on the Banana Pi natively as it doesn't take too much time. You are advised to activate the -O3 optimization:
  - `CFLAGS=-O3 make -j3`

The result is the `tnc1101` executable in the same directory

## Run simple test programs

Send and receive five test blocks at 1200 baud GFSK. The block will contain the default test phrase `Hello, World!`.

On the sending side:
  - `sudo ./picc1101 -v1 -B 9600 -P 252 -R7 -M5 -W -t7 -n5`

On the receiving side:
  - `sudo ./picc1101 -v1 -B 9600 -P 252 -R7 -M5 -W -t8-n5`

Note that you have to be super user to execute the program.

## Specify a higher priority at startup
You can use the `nice` utility: `sudo nice -n -20 ./picc1101 options...` 
This will set the priority to 0 and is the minimum you can obtain with the `nice` commmand. The lower the priority figure the higher the actual priority. 

## Blocks and packets

The CC1101 radio interface can handle blocks of up to 255 bytes in classical mode. There is an "infinite" mode supported by the CC1101 but we do not use it here. The FIFO of the CC1101 is only 64 bytes long and the process of filling or fetching data for a larger block is handled with the MSP430. 

Blocks are set as fixed size. This can create some waste at the expense of more simplicity in a code that is already complex. Moreover only fixed size blocks can benefit from the built-in forward error correction (FEC) of the CC1101. The first byte of the block is set with the size of actual useful data in the block that comes next. i.e. the counter itself is not part of the size.

However the upper layers of transmission may require to send or receive blocks larger than 255 bytes. It our terminology here this is what we call "packets". Therefore a packet may be spread over several blocks. In order to do so next to the actual data length byte which is the first byte of the block we create a "block countdown" byte that tells how many more blocks are expected to fill the packet. Thus a "block" is structured as follows:

<pre><code>
 Data count      Block countdown  Payload data
+---------------+---------------+----------------------------- 
| 1 byte        | 1 byte        | n bytes                      ...
| ex: 0x0C      | ex: 0x00      | ex: 13 bytes (0x0C + 0x01)  
+---------------+---------------+----------------------------- 
</code></pre>

This block has a countdown of 0 which means that no more blocks are expected to complete the packet. A single block has always a block countdown of 0.

## USB packets

Data and commands are sent to the MSP430 via the USB interface of the Launchpad. The structure of a block sent via USB is as follows:

<pre><code>
 Command         Size of payload  Payload 
+---------------+---------------+----------------------------- 
| 1 byte        | 1 byte        | n bytes                      ...
+---------------+---------------+----------------------------- 
</code></pre>

Commands are described by the `msp430_block_type_t` enumerated type in `common\msp430_interface.h`

<pre><code>
typedef enum msp430_block_type_e
{
    MSP430_BLOCK_TYPE_NONE = 0,
    MSP430_BLOCK_TYPE_INIT,
    MSP430_BLOCK_TYPE_TX,
    MSP430_BLOCK_TYPE_TX_KO,
    MSP430_BLOCK_TYPE_RX,
    MSP430_BLOCK_TYPE_RX_KO,
    MSP430_BLOCK_TYPE_RX_CANCEL,
    MSP430_BLOCK_TYPE_RADIO_STATUS,    
    MSP430_BLOCK_TYPE_ECHO_TEST,
    MSP430_BLOCK_TYPE_ERROR
} msp430_block_type_t;
</code></pre>

Commands are described as follows:
  - 0: MSP430_BLOCK_TYPE_NONE: Nothing, not used normally
  - 1: MSP430_BLOCK_TYPE_INIT: Initialize the CC1101 chip. Payload is the `msp430_radio_parms_t` structure type defined in `common\msp430_interface.h`
  - 2: MSP430_BLOCK_TYPE_TX: Transmit a block. Payload is the block to be transmitted
  - 3: MSP430_BLOCK_TYPE_TX_KO: When a transmission failed this block is returned to the host application by the MSP430. It contains debug data.
  - 4: MSP430_BLOCK_TYPE_RX: Receive a block. Payload is the one byte fixed block size.
  - 5: MSP430_BLOCK_TYPE_RX_KO: When a reception failed this block is returned to the host application by the MSP430. It contains debug data.
  - 6: MSP430_BLOCK_TYPE_RX_CANCEL: Cancel waiting for reception of a block. There is no payload
  - 7: MSP430_BLOCK_TYPE_RADIO_STATUS: Reads status registers. There is no transmitted payload. On return the payload contains the CC1101 registers data.
  - 8: MSP430_BLOCK_TYPE_ECHO_TEST: Do a USB echo test.
  - 9: MSP430_BLOCK_TYPE_ERROR: generic error.

The `msp430_radio_parms_t` structure is as follows:

<pre><code>
struct msp430_radio_parms_s
{
	uint8_t  packet_length;  // PACKET_LENGTH      Packet length byte
    uint8_t  packet_config;  // LENGTH_CONFIG[1:0] 2 bit packet length configuration
    uint8_t  preamble_word;  // NUM_PREAMBLE[2:0]  3 bit preamble bytes setting
    uint8_t  sync_word;      // SYNC_MODE[2:0]     3 bit synchronization mode
    uint8_t  drate_e;        // DRATE_E[3:0]       4 bit data rate exponent
    uint8_t  drate_m;        // DRATE_M[7:0]       8 bit data rate mantissa
    uint8_t  deviat_e;       // DEVIATION_E[2:0]   3 bit deviation exponent
    uint8_t  deviat_m;       // DEVIATION_M[2:0]   3 bit deviation mantissa
    uint8_t  chanbw_e;       // CHANBW_E[1:0]      2 bit channel bandwidth exponent
    uint8_t  chanbw_m;       // CHANBW_M[1:0]      2 bit channel bandwidth mantissa
    uint8_t  chanspc_e;      // CHANSPC_E[1:0]     2 bit channel spacing exponent
    uint8_t  chanspc_m;      // CHANSPC_M[7:0]     8 bit channel spacing mantissa
    uint8_t  if_word;        // FREQ_IF[4:0]       5 bit intermediate frequency word
    uint8_t  mod_word;       // MOD_FORMAT[2:0]    3 bit modulation format word
    uint8_t  fec_whitening;  // FEC (bit 0) and Data Whitening (bit 1)
    uint32_t freq_word;      // FREQ[23:0]        24 bit frequency word (FREQ0..FREQ2)
} __attribute__((packed));

typedef struct msp430_radio_parms_s msp430_radio_parms_t;
</code></pre>

Please refer to the CC1101 documentation for more information

Debug data is as follows:
  - Status byte returned by `transmit_end` or `receive_end` functions (1 byte)
  - Number of times the GDO0 interrupt was invoked on rising edge (1 byte)
  - Number of times the GDO0 interrupt was invoked on falling edge (1 byte)
  - Number of times the GDO2 interrupt was invoked on rising edge (1 byte)
  - Number of times the GDO2 interrupt was invoked on falling edge (1 byte)
  - P1 pins direction TI_CC_GDO0_PxIN. GDO0 is bit5 and GDO2 is bit4 (1 byte)
  - P1 pins interrupt flags TI_CC_GDO0_PxIFG (1 byte)
  - P1 pins interrupt enable TI_CC_GDO0_PxIE (1 byte)
  - P1 pins interrupt edge state TI_CC_GDO0_PxIES (1 byte)

## Program options
<pre><code>
Usage: tnc1101 [OPTION...] 
TNC1101 -- TNC using CC1101 module and MSP430F5529 Launchpad for the radio
link.

      --bulk-file=FILE_NAME  File name to send or receive with bulk
                             transmission (default: '-' stdin or stdout
  -B, --tnc-serial-speed=SERIAL_SPEED
                             TNC Serial speed in Bauds (default : 9600)
  -D, --tnc-serial-device=SERIAL_DEVICE
                             TNC Serial device, (default : /var/ax25/axp2)
  -f, --frequency=FREQUENCY_HZ   Frequency in Hz (default: 433600000)
  -F, --fec                  Activate FEC (default off)
  -H, --long-help            Print a long help and exit
  -I, --if-frequency=IF_FREQUENCY_HZ
                             Intermediate frequency in Hz (default: 310000)
  -l, --block-delay=DELAY_UNITS   Delay between successive radio blocks when
                             transmitting a larger block in microseconds
                             (default: 10000)
  -m, --modulation-index=MODULATION_INDEX
                             Modulation index (default 0.5)
  -M, --modulation=MODULATION_SCHEME
                             Radio modulation scheme, See long help (-H)
                             option
  -n, --repetition=REPETITION   Repetiton factor wherever appropriate, see long
                             Help (-H) option (default : 1 single)
  -p, --packet-length=PACKET_LENGTH
                             Packet length (fixed) or maximum packet length
                             (variable) (default: 250)
  -P, --large-packet-length=LARGE_PACKET_LENGTH
                             Large packet length (>255 bytes) for packet test
                             only (default: 480)
  -R, --rate=DATA_RATE_INDEX Data rate index, See long help (-H) option
  -s, --radio-status         Print radio status and exit
  -t, --tnc-mode=TNC_MODE    TNC mode of operation, See long help (-H) option
                             fpr details (default : 0)
      --tnc-keydown-delay=KEYDOWN_DELAY_US
                             FUTUR USE: TNC keydown delay in microseconds
                             (default: 0 inactive)
      --tnc-keyup-delay=KEYUP_DELAY_US
                             TNC keyup delay in microseconds (default: 10ms).
      --tnc-radio-window=RX_WINDOW_US
                             TNC time window in microseconds for concatenating
                             radio frames. 0: no concatenation (default: 0))
      --tnc-serial-window=TX_WINDOW_US
                             TNC time window in microseconds for concatenating
                             serial frames. 0: no concatenation (default:
                             40ms))
      --tnc-switchover-delay=SWITCHOVER_DELAY_US
                             FUTUR USE: TNC switchover delay in microseconds
                             (default: 0 inactive)
  -T, --real-time            Engage so called "real time" scheduling (defalut
                             0: no)
  -U, --tnc-usb-device=USB_SERIAL_DEVICE
                             Hardware TNC USB device, (default : /dev/ttyACM2
  -v, --verbose=VERBOSITY_LEVEL   Verbosiity level: 0 quiet else verbose level
                             (default : quiet)
  -V, --variable-length      Variable packet length. Given packet length
                             becomes maximum length (default off)
  -w, --rate-skew=RATE_MULTIPLIER
                             Data rate skew multiplier. (default 1.0 = no
                             skew)
  -W, --whitening            Activate whitening (default off)
  -y, --test-phrase=TEST_PHRASE   Set a test phrase to be used in test (default
                             : "Hello, World!")
  -?, --help                 Give this help list
      --usage                Give a short usage message
      --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <f4exb06@gmail.com>.
</code></pre>

Notes: 
  - variable length blocks supported by the CC1101 are not implemented.
  - inter-block delay (-l parameter) should be set to 10ms at least (-l 10000). This is the default so you may also not specify the -l parameter at all.

Example:
  - `sudo nice -n -20 ./tnc1101 -U /dev/ttyACM0 -M5 -W -p250 --tnc-keyup-delay=10000 --tnc-serial-window=10000 -l10000 -R7 -W -D/var/slip/slip2 -v3 -t3`

### Modulation index and deviation
The program tries to find the closest value for the specified baud rate in binary symbol modulation (i.e. 2-FSK). For example at 600 baud rate and modulation index of 8 the deviation is 4761 Hz thus approachong 600*8 = 4800 Hz. As per CC1101 specifications a 4-FSK modulation falls in the same bandwidth. The following figure shows how 2-FSK and 4-FSK signals fit in the same bandwidth:
<pre><code>
2-FSK:
0                 1
^        f0       ^
|        ^        |
|        |        |
|        |        |
|        |        |
+--.--.--.--.--.--+
|\<        \>|
 deviation

4-FSK:
01    00    10    11
^     ^  f0 ^     ^
|     |  ^  |     |
|     |  |  |     |
|     |  |  |     |
|     |  |  |     |
+--.--+--.--+--.--+
|     |
2/3 deviation
</code></pre>

## Detailed options
### Verbosity level (-v)
It ranges from 0 to 4:
  - 0: nothing at all
  - 1: Errors and some warnings and one line summary for each block sent or received
  - 2: Adds details on received blocks like RSSI and LQI
  - 3: Adds full hex dump of received blocks
  - 4: Adds full hex dump of sent blocks

Be aware that printing out to console takes time and might cause problems when transfer speeds and interactivity increase.

### Radio interface speeds (-R)
 <pre><code>
Value: Rate (Baud):
 0     50 (experimental)
 1     110 (experimental)
 2     300 (experimental)
 3     600
 4     1200
 5     2400
 6     4800
 7     9600
 8     14400
 9     19200
10     28800
11     38400
12     57600
13     76800
14     115200
15     250000
16     500000 (300000 for 4-FSK)
</code></pre>

### Modulations (-M)
 <pre><code>
Value: Scheme:
0      OOK
1      2-FSK
2      4-FSK
3      MSK
4      GFSK
</code></pre>

Note: MSK does not seem to work too well at least with the default radio options.

### TNC mode (-t)
 <pre><code>
Value: Scheme:
0	File bulk transmission
1	File bulk reception
2	KISS TNC
3	SLIP TNC
4	USB echo
5	Radio status
6	Radio init
7	Radio block transmission test
8	Radio block reception test
9	Radio block echo test starting with Tx
10	Radio block echo test starting with Rx
11	Radio packet transmission test
12	Radio packet reception test
13	Radio packet reception test in non-blocking mode
</code></pre>

#AX.25/KISS operation

The AX.25/KISS protocol is handled natively in Linux. You must have compiled your kernel with AX.25 and KISS support as modules. The following modules have to be loaded:
  - `ax25`
  - `mkiss`

You must have a few packages installed. Make sure they are installed:
  - `sudo apt-get install ax25-apps ax25-node ax25-tools libax25 socat`

In the old days you would interface the TNC with a physical serial device and cable. However in the present case the TNC is made of software (as seen from the host) so we will use virtual serial devices and a virtual serial link. On end of the link will be used by the AX.25/KISS layer and the other end will be used by the virtual TNC software. This virtual link is set-up thanks to the socat utility:
  - `sudo socat -d -d pty,link=/var/ax25/axp1,raw,echo=0 pty,link=/var/ax25/axp2,raw,echo=0 &`

This creates two virtual serial devices linked via a virtual serial cable:
  - `/var/ax25/axp1`
  - `/var/ax25/axp2`

`/var/ax25/axp1` is used by the AX.25/KISS layer and `/var/ax25/axp2` is used by the virtual TNC.

You must configure the serial interface used by AX.25/KISS in the `/etc/ax25/axports` file. You have to add a line there to describe your TNC device. Please make sure there are no blank lines in the file. Commented lines use the # character in first position. If you need blank lines make sure you have a comment character in first position. The line is composed of the following fields:
  - `<interface name> <callsign and suffix> <speed> <paclen> <window size> <comment>`
  - *interface name* is any name you will refer this interface to later
  - *callsign and suffix* is your callsign and a suffix from 0 to 15. Ex: `F4EXB-14` and is the interface hardware address for AX.25 just like the MAC address is the hardware address for Ethrnet.
  - *speed* is the speed in Baud. This has not been found really effective. The speed will be determined by the settings of the CC1101 itself and the TCP/IP flow will adapt to the actual speed.
  - *paclen* this is the MTU of the network interface (ax0). Effectively this sets the limit on the size of each individual KISS frame although several frames can be concatenated. The value 224 along with a fixed radio block size (-P parameter) of 252 has been found satisfactory in most conditions.  
  - *window size* is a number from 1 to 7 and is the maximum number of packets before an acknowledgement is required. This doesn't really work with KISS. KISS determines how many packets can be combined together in concatenated KISS frames that are sent as a single block. On the other end of the transmission the ACK can only be returned after the whole block has been received.
  - *comment* is any descriptive comment

Example:
 <pre><code>
 # /etc/ax25/axports
 #
 # The format of this file is:
 #
 # name callsign speed paclen window description
 #
 radio0  F4EXB-14           9600  224     1       Hamnet CC1101
 radio1  F4EXB-15           9600  224     1       Hamnet CC1101
 #1      OH2BNS-1           1200  255     2       144.675 MHz (1200  bps)
 #2      OH2BNS-9          38400  255     7       TNOS/Linux  (38400 bps)
</code></pre>

The AX.25/KISS network interface is created with the `kissattach` command and the netmask is specified with `/sbin/ifconfig` command:
  - `sudo kissattach /var/ax25/axp1 radio0 10.0.1.7`
  - `sudo ifconfig ax0 netmask 255.255.255.0`


To set-up the AX.25/KISS connection you can use the `kissup.sh` script found in the scripts folder. Your user must be sudoer. Example:
  - `./kissup.sh radio0 10.0.1.7 255.255.255.0`

To bring down the AX.25/KISS connection you can use the `kissdown.sh` script that takes no parameter:
  - `./kissdown.sh`

#SLIP operation

This is very similar to AX.25/KISS. The main difference for the tnc1101 program is that there are no commands sent to the TNC therefore the byte following the 0xC0 delimiter should not be interpreted. This mode is activated with the -t3 option.

Here we also make use of a virtual serial cable between two virtual serial interfaces created via socat. To make the distinction with AX.25/KISS they are named differently:
  - `/var/slip/slip1`
  - `/var/slip/slip2`

You must have the SLIP support compiled as modules in your kernel. The following modules have to be loaded: 
  - `slip.ko`
  - `slhc.ko`

You must also install slattach found in the net-tools package. This may be already installed as essential tools like `route` or `ifconfig` are part of this package. In any case try the following:
  - `sudo apt-get install net-tools`

To set-up a SLIP connection you can use the slipup.sh script in the scripts folder. You must be sudoer. It takes the IP address of the host as the first argument and the IP address of the distant host as the second argument. SLIP links both hosts individually in a point to point connection therefore the assumed netmask is 255.255.255.255.

Example:
<pre><code>
./slipup.sh 10.0.2.1 10.0.2.2
2015/04/09 10:36:50 socat[12561] N PTY is /dev/pts/1
2015/04/09 10:36:50 socat[12561] N PTY is /dev/pts/2
2015/04/09 10:36:50 socat[12561] N starting data transfer loop with FDs [5,5] and [7,7]

/sbin/ifconfig
...
sl0       Link encap:Serial Line IP  
      inet addr:10.0.2.1  P-t-P:10.0.2.2  Mask:255.255.255.255
      UP POINTOPOINT RUNNING NOARP MULTICAST  MTU:296  Metric:1
      RX packets:0 errors:0 dropped:0 overruns:0 frame:0
      TX packets:0 errors:0 dropped:0 overruns:0 carrier:0
      collisions:0 txqueuelen:10 
      RX bytes:0 (0.0 B)  TX bytes:0 (0.0 B)
</code></pre>

To undo the SLIP connection set-up you can use the slipdown.sh script in the scripts folder. 

Example:
<pre><code>
./slipdown.sh 
2015/04/09 10:38:10 socat[12820] N socat_signal(): handling signal 15
2015/04/09 10:38:10 socat[12820] W exiting on signal 15
2015/04/09 10:38:10 socat[12820] N socat_signal(): finishing signal 15
2015/04/09 10:38:10 socat[12820] N exit(143)
</code></pre>
