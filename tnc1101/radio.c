/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* Radio link interface                                                       */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"
#include "util.h"
#include "radio.h"
#include "serial.h"
#include "msp430_interface.h"

char *state_names[] = {
    "SLEEP",            // 00
    "IDLE",             // 01
    "XOFF",             // 02
    "VCOON_MC",         // 03
    "REGON_MC",         // 04
    "MANCAL",           // 05
    "VCOON",            // 06
    "REGON",            // 07
    "STARTCAL",         // 08
    "BWBOOST",          // 09
    "FS_LOCK",          // 10
    "IFADCON",          // 11
    "ENDCAL",           // 12
    "RX",               // 13
    "RX_END",           // 14
    "RX_RST",           // 15
    "TXRX_SWITCH",      // 16
    "RXFIFO_OVERFLOW",  // 17
    "FSTXON",           // 18
    "TX",               // 19
    "TX_END",           // 20
    "RXTX_SWITCH",      // 21
    "TXFIFO_UNDERFLOW", // 22
    "undefined",        // 23
    "undefined",        // 24
    "undefined",        // 25
    "undefined",        // 26
    "undefined",        // 27
    "undefined",        // 28
    "undefined",        // 29
    "undefined",        // 30
    "undefined"         // 31
};

// 4x4 channel bandwidth limits
float chanbw_limits[] = {
    812000.0,
    650000.0,
    541000.0,
    464000.0,
    406000.0,
    325000.0,
    270000.0,
    232000.0,
    203000.0,
    162000.0,
    135000.0,
    116000.0,
    102000.0,
    81000.0,
    68000.0,
    58000.0
};

#define DATA_BUFFER_SIZE 257

uint8_t  dataBuffer[DATA_BUFFER_SIZE];
uint8_t  ackBuffer[DATA_BUFFER_SIZE];
uint32_t blocks_sent;
uint32_t blocks_received;
uint32_t packets_sent;
uint32_t packets_received;

// === Static functions declarations ==============================================================
static uint32_t get_freq_word(arguments_t *arguments);
static uint8_t  get_mod_word(radio_modulation_t modulation_code);
static radio_modulation_t get_mod_code(uint8_t mod_word);
static uint8_t  get_if_word(arguments_t *arguments);
static void     get_chanbw_words(float bw, msp430_radio_parms_t *radio_parms);
static void     get_rate_words(arguments_t *arguments, msp430_radio_parms_t *radio_parms);
static int      read_usb(serial_t *serial_parms, uint8_t *dataBuffer, int size, uint32_t timeout);
/*
static void     wait_for_state(spi_parms_t *spi_parms, ccxxx0_state_t state, uint32_t timeout);
static void     print_received_packet(int verbose_min);
static void     radio_send_block(spi_parms_t *spi_parms, uint8_t block_countdown);
static uint8_t  radio_receive_block(spi_parms_t *spi_parms, arguments_t *arguments, uint8_t *block, uint32_t *size, uint8_t *crc);
static uint8_t  crc_check(uint8_t *block);
*/

// === Static functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Calculate frequency word FREQ[23..0]
uint32_t get_freq_word(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint64_t res; // calculate on 64 bits to save precision
    res = ((uint64_t) (arguments->freq_hz) * (uint64_t) (1<<16)) / ((uint64_t) (F_XTAL_MHZ * 1000000ULL));
    return (uint32_t) res;
}

// ------------------------------------------------------------------------------------------------
// Calculate intermediate frequency word FREQ_IF[4:0]
uint8_t get_if_word(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t freq_xtal = F_XTAL_MHZ * 1000000;
    return (uint8_t) ((arguments->if_freq_hz * (1<<10)) / freq_xtal) % 32;
}

// ------------------------------------------------------------------------------------------------
// Calculate modulation format word MOD_FORMAT[2..0]
uint8_t get_mod_word(radio_modulation_t modulation_code)
// ------------------------------------------------------------------------------------------------
{
    switch (modulation_code)
    {
        case RADIO_MOD_OOK:
            return 3;
            break;
        case RADIO_MOD_FSK2:
            return 0;
            break;
        case RADIO_MOD_FSK4:
            return 4;
            break;
        case RADIO_MOD_MSK:
            return 7;
            break;
        case RADIO_MOD_GFSK:
            return 1;
            break;
        default:
            return 0;
    }
}

// ------------------------------------------------------------------------------------------------
radio_modulation_t get_mod_code(uint8_t mod_word)
// ------------------------------------------------------------------------------------------------
{
    switch (mod_word)
    {
        case 3:
            return RADIO_MOD_OOK;
            break;
        case 0:
            return RADIO_MOD_FSK2;
            break;
        case 4:
            return RADIO_MOD_FSK4;
            break;
        case 7:
            return RADIO_MOD_MSK;
            break;
        case 1:
            return RADIO_MOD_GFSK;
            break;
        default:
            return RADIO_MOD_NONE;
    }
}

// ------------------------------------------------------------------------------------------------
// Calculate CHANBW words according to CC1101 bandwidth steps
void get_chanbw_words(float bw, msp430_radio_parms_t *radio_parms)
// ------------------------------------------------------------------------------------------------
{
    uint8_t e_index, m_index;

    for (e_index=0; e_index<4; e_index++)
    {
        for (m_index=0; m_index<4; m_index++)
        {
            if (bw > chanbw_limits[4*e_index + m_index])
            {
                radio_parms->chanbw_e = e_index;
                radio_parms->chanbw_m = m_index;
                return;
            }
        }
    }

    radio_parms->chanbw_e = 3;
    radio_parms->chanbw_m = 3;
}

// ------------------------------------------------------------------------------------------------
// Calculate data rate, channel bandwidth and deviation words. Assumes 26 MHz crystal.
//   o DRATE = (Fxosc / 2^28) * (256 + DRATE_M) * 2^DRATE_E
//   o CHANBW = Fxosc / (8(4+CHANBW_M) * 2^CHANBW_E)
//   o DEVIATION = (Fxosc / 2^17) * (8 + DEVIATION_M) * 2^DEVIATION_E
void get_rate_words(arguments_t *arguments, msp430_radio_parms_t *radio_parms)
// ------------------------------------------------------------------------------------------------
{
    double drate, deviat, f_xtal;

    drate = (double) rate_values[arguments->rate];
    drate *= arguments->rate_skew;

    if ((arguments->modulation == RADIO_MOD_FSK4) && (drate > 300000.0))
    {
        fprintf(stderr, "RADIO: forcibly set data rate to 300 kBaud for 4-FSK\n");
        drate = 300000.0;
    }

    deviat = drate * arguments->modulation_index;
    f_xtal = (double) F_XTAL_MHZ * 1e6;

    get_chanbw_words(2.0*(deviat + drate), radio_parms); // Apply Carson's rule for bandwidth

    radio_parms->drate_e = (uint8_t) (floor(log2( drate*(1<<20) / f_xtal )));
    radio_parms->drate_m = (uint8_t) (((drate*(1<<28)) / (f_xtal * (1<<radio_parms->drate_e))) - 256);
    radio_parms->drate_e &= 0x0F; // it is 4 bits long

    radio_parms->deviat_e = (uint8_t) (floor(log2( deviat*(1<<14) / f_xtal )));
    radio_parms->deviat_m = (uint8_t) (((deviat*(1<<17)) / (f_xtal * (1<<radio_parms->deviat_e))) - 8);
    radio_parms->deviat_e &= 0x07; // it is 3 bits long
    radio_parms->deviat_m &= 0x07; // it is 3 bits long
}

// ------------------------------------------------------------------------------------------------
// Read USB with timeout
// Timeout is in 10's of microseconds
int read_usb(serial_t *serial_parms, uint8_t *buffer, int size, uint32_t timeout_value)
// ------------------------------------------------------------------------------------------------
{
    int      nbytes;
    uint32_t timeout = timeout_value;
    uint8_t  block_size = 0, byte_count = 0;

    // data block may span over many USB blocks

    do
    {
        do // attempt to read one block
        {
            nbytes = read_serial(serial_parms, &buffer[byte_count], size);
            usleep(10);
            timeout--;
        } while ((nbytes <= 0) && ((timeout_value == 0) || (timeout > 0)));

        if (nbytes > 0) // accumulate
        {
            byte_count += nbytes;
        }

        if ((byte_count >= 2) && (block_size == 0)) // get the block size. End condition is when block size plus header is received.
        {
            block_size = buffer[1];
        }

    } while((byte_count < block_size+2) && ((timeout_value == 0) || (timeout > 0)));

    if (nbytes > 0)
    {
        return byte_count;
    }
    else
    {
        return nbytes;
    }
}

/*
// ------------------------------------------------------------------------------------------------
// Poll FSM state waiting for given state until timeout (approx ms)
void wait_for_state(spi_parms_t *spi_parms, ccxxx0_state_t state, uint32_t timeout)
// ------------------------------------------------------------------------------------------------
{
    uint8_t fsm_state;

    while(timeout)
    {
        PI_CC_SPIReadStatus(spi_parms, PI_CCxxx0_MARCSTATE, &fsm_state);
        fsm_state &= 0x1F;

        if (fsm_state == (uint8_t) state)
        {
            break;
        }

        usleep(1000);
        timeout--;
    }

    if (!timeout)
    {
        verbprintf(1, "RADIO: timeout reached in state %s waiting for state %s\n", state_names[fsm_state], state_names[state]);
        
        if (fsm_state == CCxxx0_STATE_RXFIFO_OVERFLOW)
        {
            PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SFRX); // Flush Rx FIFO
            PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SFTX); // Flush Tx FIFO
        }
    }    
}

// ------------------------------------------------------------------------------------------------
void print_received_packet(int verbose_min)
// Print a received packet stored in the interrupt data block
// ------------------------------------------------------------------------------------------------
{
    uint8_t rssi_dec, crc_lqi;
    int i;

    verbprintf(verbose_min, "Rx: packet length %d, FIFO was hit %d times\n", 
        radio_int_data.rx_count,
        radio_int_data.threshold_hits);
    print_block(verbose_min+2, (uint8_t *) radio_int_data.rx_buf, radio_int_data.rx_count);

    rssi_dec = radio_int_data.rx_buf[radio_int_data.rx_count-2];
    crc_lqi  = radio_int_data.rx_buf[radio_int_data.rx_count-1];
    radio_int_data.rx_buf[radio_int_data.rx_count-2] = '\0';

    verbprintf(verbose_min, "%d: (%03d) \"%s\"\n", radio_int_data.rx_buf[1], radio_int_data.rx_buf[0] - 1, &radio_int_data.rx_buf[2]);
    verbprintf(verbose_min, "RSSI: %.1f dBm. LQI=%d. CRC=%d\n", 
        rssi_dbm(rssi_dec),
        0x7F - (crc_lqi & 0x7F),
        (crc_lqi & PI_CCxxx0_CRC_OK)>>7);
}
*/

// === Public functions ===========================================================================
/*
// ------------------------------------------------------------------------------------------------
// Initialize interrupt data and mechanism
void init_radio_int(spi_parms_t *spi_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    radio_int_data.mode = RADIOMODE_NONE;
    radio_int_data.packet_rx_count = 0;
    radio_int_data.packet_tx_count = 0;
    packets_sent = 0;
    packets_received = 0;
    radio_int_data.spi_parms = spi_parms;
    radio_int_data.wait_us = 8000000 / rate_values[arguments->rate]; // approximately 2-FSK byte delay
    p_radio_int_data = &radio_int_data;

    wiringPiISR(WPI_GDO0, INT_EDGE_BOTH, &int_packet);       // set interrupt handler for packet interrupts

    if (arguments->packet_length >= PI_CCxxx0_FIFO_SIZE)
    {
        wiringPiISR(WPI_GDO2, INT_EDGE_BOTH, &int_threshold); // set interrupt handler for FIFO threshold interrupts
    }

    verbprintf(1, "Unit delay .............: %d us\n", radio_int_data.wait_us);
    verbprintf(1, "Packet delay ...........: %d us\n", arguments->packet_delay * radio_int_data.wait_us);
}

// ------------------------------------------------------------------------------------------------
// Inhibit operations by returning to IDLE state
void radio_turn_idle(spi_parms_t *spi_parms)
// ------------------------------------------------------------------------------------------------
{
    PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SIDLE);
}

// ------------------------------------------------------------------------------------------------
// Allow Rx operations by returning to Rx state
void radio_turn_rx(spi_parms_t *spi_parms)
// ------------------------------------------------------------------------------------------------
{
    PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SRX);
    wait_for_state(spi_parms, CCxxx0_STATE_RX, 10); // Wait max 10ms
}

// ------------------------------------------------------------------------------------------------
// Flush Rx and Tx FIFOs
void radio_flush_fifos(spi_parms_t *spi_parms)
// ------------------------------------------------------------------------------------------------
{
    PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SFRX); // Flush Rx FIFO
    PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SFTX); // Flush Tx FIFO
}
*/

// ------------------------------------------------------------------------------------------------
// Get the time to transmit or receive a byte in microseconds
float radio_get_byte_time(msp430_radio_parms_t *radio_parms)
// ------------------------------------------------------------------------------------------------
{
    float base_time = 8000000.0 / radio_get_rate(radio_parms);

    if (get_mod_code(radio_parms->mod_word) == RADIO_MOD_FSK4)
    {
        base_time /= 2.0;
    }

    if (radio_parms->fec_whitening & 0x01)
    {
        base_time *= 2.0;
    }

    return base_time;
}

// ------------------------------------------------------------------------------------------------
// Get the actual data rate in Bauds
float radio_get_rate(msp430_radio_parms_t *radio_parms)
// ------------------------------------------------------------------------------------------------
{
    float f_xtal = F_XTAL_MHZ * 1e6;

    return ((float) (f_xtal) / (1<<28)) * (256 + radio_parms->drate_m) * (1<<radio_parms->drate_e);
}

// ------------------------------------------------------------------------------------------------
// Initialize MSP430-CC1101 radio parameters
void init_radio_parms(msp430_radio_parms_t *radio_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    get_rate_words(arguments, radio_parms); // Bitrate
    radio_parms->if_word       = get_if_word(arguments);    
    radio_parms->freq_word     = get_freq_word(arguments);
    radio_parms->mod_word      = get_mod_word(arguments->modulation);
    radio_parms->sync_word     = SYNC_30_over_32;  // 30/32 sync word bits detected
    radio_parms->chanspc_m     = 0;                // Do not use channel spacing for the moment defaulting to 0
    radio_parms->chanspc_e     = 0;                // Do not use channel spacing for the moment defaulting to 0
    radio_parms->fec_whitening = arguments->fec + 2*arguments->whitening;
    radio_parms->packet_length = arguments->packet_length;  // Packet length
    radio_parms->preamble_word = nb_preamble_bytes[(int) arguments->preamble]; // set number of preamble bytes

    if (arguments->variable_length)
    {
        radio_parms->packet_config = PKTLEN_VARIABLE;  // Use variable packet length
    }
    else
    {
        radio_parms->packet_config = PKTLEN_FIXED;     // Use fixed packet length
    }
}

// ------------------------------------------------------------------------------------------------
// Initialize the radio link interface
int init_radio(serial_t *serial_parms, msp430_radio_parms_t *radio_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    int nbytes;

    dataBuffer[0] = (uint8_t) MSP430_BLOCK_TYPE_INIT;
    dataBuffer[1] = sizeof(msp430_radio_parms_t);
    memcpy(&dataBuffer[2], radio_parms, dataBuffer[1]);

    verbprintf(1, "init_radio...\n");

    nbytes = write_serial(serial_parms, dataBuffer, dataBuffer[1]+2);
    verbprintf(1, "%d bytes written to USB\n", nbytes);

    nbytes = read_usb(serial_parms, dataBuffer, DATA_BUFFER_SIZE, 10000);

    print_block(3, dataBuffer, nbytes);

    return nbytes;
}

// ------------------------------------------------------------------------------------------------
// Print status registers to stderr
void print_radio_status(serial_t *serial_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint8_t *regs;
    int nbytes;

    dataBuffer[0] = (uint8_t) MSP430_BLOCK_TYPE_RADIO_STATUS;
    dataBuffer[1] = 0;

    fprintf(stderr, "Start...\n");

    nbytes = write_serial(serial_parms, dataBuffer, 2);
    verbprintf(1, "%d bytes written to USB\n", nbytes);

    nbytes = read_usb(serial_parms, dataBuffer, DATA_BUFFER_SIZE, 10000);

    print_block(3, dataBuffer, nbytes);

    if (nbytes >= 14)
    {
        regs = &dataBuffer[2];

        fprintf(stderr, "Part number ...........: %d\n", regs[0]);
        fprintf(stderr, "Version ...............: %d\n", regs[1]);
        fprintf(stderr, "Freq offset estimate ..: %d\n", regs[2]);
        fprintf(stderr, "CRC OK ................: %d\n", ((regs[3] & 0x80)>>7));
        fprintf(stderr, "LQI ...................: %d\n", regs[3] & 0x7F);
        fprintf(stderr, "RSSI ..................: %.1f dBm\n", rssi_dbm(regs[4]));
        fprintf(stderr, "Radio FSM state .......: %s\n", state_names[regs[5] & 0x1F]);
        fprintf(stderr, "WOR time ..............: %d\n", ((regs[6] << 8) + regs[7]));
        fprintf(stderr, "Carrier Sense .........: %d\n", ((regs[8] & 0x40)>>6));
        fprintf(stderr, "Preamble Qual Reached .: %d\n", ((regs[8] & 0x20)>>5));
        fprintf(stderr, "Clear channel .........: %d\n", ((regs[8] & 0x10)>>4));
        fprintf(stderr, "Start of frame delim ..: %d\n", ((regs[8] & 0x08)>>3));
        fprintf(stderr, "GDO2 ..................: %d\n", ((regs[8] & 0x04)>>2));
        fprintf(stderr, "GDO0 ..................: %d\n", ((regs[8] & 0x01)));
        fprintf(stderr, "VCO VC DAC ............: %d\n", regs[9]);
        fprintf(stderr, "FIFO Tx underflow .....: %d\n", ((regs[10] & 0x80)>>7));
        fprintf(stderr, "FIFO Tx bytes .........: %d\n", regs[10] & 0x7F);
        fprintf(stderr, "FIFO Rx overflow ......: %d\n", ((regs[11] & 0x80)>>7));
        fprintf(stderr, "FIFO Rx bytes .........: %d\n", regs[11] & 0x7F);
    }
    else
    {
        fprintf(stderr, "Error reading from %s\n", arguments->usbacm_device);
    }

}

// ------------------------------------------------------------------------------------------------
// Print actual radio link parameters once initialized
//   o Operating frequency ..: Fo   = (Fxosc / 2^16) * FREQ[23..0]
//   o Channel spacing ......: Df   = (Fxosc / 2^18) * (256 + CHANSPC_M) * 2^CHANSPC_E
//   o Channel bandwidth ....: BW   = Fxosc / (8 * (4+CHANBW_M) * 2^CHANBW_E)
//   o Data rate (Baud) .....: Rate = (Fxosc / 2^28) * (256 + DRATE_M) * 2^DRATE_E
//   o Deviation ............: Df   = (Fxosc / 2^17) * (8 + DEVIATION_M) * 2^DEVIATION_E

void print_radio_parms(msp430_radio_parms_t *radio_parms)
// ------------------------------------------------------------------------------------------------
{
    float f_xtal = F_XTAL_MHZ * 1e6;

    fprintf(stderr, "\n--- Actual radio channel parameters ---\n");
    fprintf(stderr, "Operating frequency ....: %.3f MHz (W=%d)\n", 
        ((f_xtal/1e6) / (1<<16))*radio_parms->freq_word, radio_parms->freq_word);
    fprintf(stderr, "Intermediate frequency .: %.3f kHz (W=%d)\n", 
        ((f_xtal/1e3) / (1<<10))*radio_parms->if_word, radio_parms->if_word);
    fprintf(stderr, "Channel spacing ........: %.3f kHz (M=%d, E=%d)\n", 
        ((f_xtal/1e3) / (1<<18))*(256+radio_parms->chanspc_m)*(1<<radio_parms->chanspc_e), radio_parms->chanspc_m, radio_parms->chanspc_e);
    fprintf(stderr, "Channel bandwidth.......: %.3f kHz (M=%d, E=%d)\n",
        (f_xtal/1e3) / (8*(4+radio_parms->chanbw_m)*(1<<radio_parms->chanbw_e)), radio_parms->chanbw_m, radio_parms->chanbw_e);
    fprintf(stderr, "Data rate ..............: %.1f Baud (M=%d, E=%d)\n",
        radio_get_rate(radio_parms), radio_parms->drate_m, radio_parms->drate_e);
    fprintf(stderr, "Deviation ..............: %.3f kHz (M=%d, E=%d)\n",
        ((f_xtal/1e3) / (1<<17)) * (8 + radio_parms->deviat_m) * (1<<radio_parms->deviat_e), radio_parms->deviat_m, radio_parms->deviat_e);
    fprintf(stderr, "Modulation word ........: %d (%s)\n", 
        radio_parms->mod_word, modulation_names[get_mod_code(radio_parms->mod_word)]);
    fprintf(stderr, "FEC ....................: %s\n", (radio_parms->fec_whitening & 0x01 ? "on" : "off"));
    fprintf(stderr, "Data whitening .........: %s\n", (radio_parms->fec_whitening & 0x02 ? "on" : "off"));
    fprintf(stderr, "Packet length ..........: %d bytes\n",
        radio_parms->packet_length);
    fprintf(stderr, "Byte time ..............: %d us\n",
        ((uint32_t) radio_get_byte_time(radio_parms)));
    fprintf(stderr, "Packet time ............: %d us\n",
        (uint32_t) (radio_parms->packet_length * radio_get_byte_time(radio_parms)));
}

// ------------------------------------------------------------------------------------------------
// Send a radio block of maximum 255 bytes
// dataBlock      is the start of the actual data block
// dataSize       is the size of the useful data inside the block
// blockCountdown is the block countdown provisoned for multi-block packets
// blockSize      is the radio block size
// ackblock       is the acknowledgement block
// ackBlockSize   (input)  is the acknowledgement block allocated size
//                (output) is the actual acknowledgement block size
// timeout_us     is the acknowledgement timeout in microseconds
// returns        the number of bytes sent over USB
int radio_send_block(serial_t *serial_parms, 
        uint8_t  *dataBlock, 
        uint8_t  dataSize,
        uint8_t  blockCountdown, 
        uint8_t  blockSize,
        uint8_t  *ackBlock, 
        int      *ackBlockSize, 
        uint32_t timeout_us)
// ------------------------------------------------------------------------------------------------
{
    int nbytes, ackbytes;

    dataBuffer[0] = (uint8_t) MSP430_BLOCK_TYPE_TX;
    dataBuffer[1] = blockSize;
    dataBuffer[2] = dataSize;
    dataBuffer[3] = blockCountdown;
    memcpy(&dataBuffer[4], dataBlock, blockSize-2);

    nbytes = write_serial(serial_parms, dataBuffer, blockSize+2);
    verbprintf(2, "Block (%d,%d): %d bytes written to USB\n",
        dataBuffer[2],
        dataBuffer[3],
        nbytes);

    ackbytes = read_usb(serial_parms, ackBlock, *ackBlockSize, timeout_us/10);
    *ackBlockSize = ackbytes;

    return nbytes;
}

// ------------------------------------------------------------------------------------------------
// Transmission of a packet
void radio_send_packet(serial_t *serial_parms,
        uint8_t  *packet,
        uint8_t  blockSize,
        uint32_t size,
        uint32_t block_delay_us,
        uint32_t block_timeout_us)
// ------------------------------------------------------------------------------------------------
{
    int     block_countdown = size / (blockSize - 2);
    uint8_t *block_start = packet;
    uint8_t data_length;
    uint8_t ackBuffer[DATA_BUFFER_SIZE];
    int     nbytes, ackbytes;

    while (block_countdown >= 0)
    {
        data_length = (size > blockSize - 2 ? blockSize - 2 : size);
        ackbytes = DATA_BUFFER_SIZE; 

        nbytes = radio_send_block(serial_parms, 
            packet,
            data_length + 1, // size takes countdown counter into account
            block_countdown,
            blockSize,
            ackBuffer,
            &ackbytes,
            block_timeout_us);

        verbprintf(2, "Block (%d,%d): %d bytes sent %d bytes received from radio_send_block\n", 
            data_length + 1,
            block_countdown, 
            nbytes, 
            ackbytes); 
        
        if (ackbytes > 0)
        {
            print_block(3, ackBuffer, ackbytes);
        }        

        block_start += data_length;
        size -= data_length;
        block_countdown--;

        if (block_delay_us)
        {
            usleep(block_delay_us); // pause before sending the next block
        }
    }
}

// ------------------------------------------------------------------------------------------------
// Receive of a block
// dataBlock      is the pointer to the actual data
// dataBlockSize  is the size of the radio block
// blockCountdown is the block countdown
// size           is incremented by the size of the actual data
// rssi           is updated with the RSSI byte
// crc_lqi        is updated with the CRC+LQI combination byte
// timeout_us     is the timeout in microseconds to receive block
// Returns the number of bytes read from USB. It has to be greater than 4 for data to be valid
int radio_receive_block(serial_t *serial_parms, 
        uint8_t  *dataBlock,
        uint8_t  dataBlockSize,
        uint8_t  *blockCountdown,
        uint32_t *size, 
        uint8_t  *rssi,
        uint8_t  *crc_lqi,
        uint32_t timeout_us)
// ------------------------------------------------------------------------------------------------
{
    int     nbytes;
    uint8_t block_size;
    uint8_t data_size;

    dataBuffer[0] = (uint8_t) MSP430_BLOCK_TYPE_RX;
    dataBuffer[1] = dataBlockSize;

    nbytes = write_serial(serial_parms, dataBuffer, 2);
    verbprintf(2, "%d bytes written to USB\n", nbytes);

    nbytes = read_usb(serial_parms, dataBuffer, DATA_BUFFER_SIZE, timeout_us/10);
    verbprintf(2, "%d bytes read from USB\n", nbytes);

    if (nbytes > 0)
    {
        print_block(3, dataBuffer, nbytes);
    }

    if (nbytes >= 4)
    {
        block_size = dataBuffer[1]; // complete size less command byte and counter itself (2 bytes)
        data_size = dataBuffer[2] - 1;
        *blockCountdown = dataBuffer[3]; // block countdown
        *size += data_size;

        if (nbytes > 4) // some data is returned
        {
            memcpy(dataBlock, &dataBuffer[4], data_size);
        }

        if (nbytes >= 6)
        {
            *rssi    = dataBuffer[block_size+2 - 2]; // byte before last byte is RSSI
            *crc_lqi = dataBuffer[block_size+2 - 1]; // last byte is CRC+LQI combination byte
        }
    }

    return nbytes;
}

// ------------------------------------------------------------------------------------------------
// Receive of a packet
// packet    is the pointer to the reception area
// blockSize is the size of the radio block
// ..timeout is the timoeut in microseconds between successive blocks
// Returns   actual data size
uint32_t radio_receive_packet(serial_t *serial_parms,
    uint8_t  *packet,
    uint8_t  blockSize,
    uint32_t inter_block_timeout_us)
// ------------------------------------------------------------------------------------------------
{
    int      nbytes;
    uint8_t  crc_lqi, crc, lqi, rssi, block_countdown, block_count = 0;
    uint32_t packet_size = 0;
    uint32_t timeout = 0;

    do
    {
        nbytes = radio_receive_block(serial_parms, 
            &packet[packet_size],
            blockSize,
            &block_countdown,
            &packet_size,
            &rssi,
            &crc_lqi,
            timeout);

        if (nbytes <= 4) // >4 for data to be valid. If not consider it's a timeout.
        {
            verbprintf(1, "RADIO: timeout trying to read the next block. Aborting packet\n");
            packet[0] = '\0';
            return 0;
        }

        if (!block_count)
        {
            block_count = block_countdown + 1;
            timeout = inter_block_timeout_us;
        }

        block_count--;

        if (block_count != block_countdown)
        {
            verbprintf(1, "RADIO: block sequence error. Aborting packet\n");
            packet[0] = '\0';
            return 0;
        }

        crc = get_crc_lqi(crc_lqi, &lqi);

        if (crc)
        {
            verbprintf(2, "Block countdown: %d Size so far: %d RSSI: %.1f dBm CRC OK\n",
                block_countdown, 
                packet_size, 
                rssi_dbm(rssi));
        }
        else
        {
            verbprintf(1, "RADIO: CRC error, aborting packet\n");
            packet[0] = '\0';
            return 0;
        }

    } while (block_countdown > 0);

    return packet_size;
}

/*
// ------------------------------------------------------------------------------------------------
// Set packet length
int radio_set_packet_length(spi_parms_t *spi_parms, uint8_t pkt_len)
// ------------------------------------------------------------------------------------------------
{
    return PI_CC_SPIWriteReg(spi_parms, PI_CCxxx0_PKTLEN, pkt_len); // Packet length.
}

// ------------------------------------------------------------------------------------------------
// Get packet length
uint8_t radio_get_packet_length(spi_parms_t *spi_parms)
// ------------------------------------------------------------------------------------------------
{
    uint8_t pkt_len;
    PI_CC_SPIReadReg(spi_parms, PI_CCxxx0_PKTLEN, &pkt_len); // Packet length.
    return pkt_len;
}
*/

/*
// ------------------------------------------------------------------------------------------------
// Wait for approximately an amount of 2-FSK symbols bytes
void radio_wait_a_bit(uint32_t amount)
// ------------------------------------------------------------------------------------------------
{
    usleep(amount * radio_int_data.wait_us);
}

// ------------------------------------------------------------------------------------------------
// Wait for the reception or transmission to finish
void radio_wait_free()
// ------------------------------------------------------------------------------------------------
{
    while((radio_int_data.packet_receive) || (radio_int_data.packet_send))
    {
        radio_wait_a_bit(4);
    }
}

// ------------------------------------------------------------------------------------------------
// Initialize for Rx mode
void radio_init_rx(spi_parms_t *spi_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    blocks_received = radio_int_data.packet_rx_count;
    radio_int_data.mode = RADIOMODE_RX;
    radio_int_data.packet_receive = 0;    
    radio_int_data.threshold_hits = 0;
    radio_set_packet_length(spi_parms, arguments->packet_length);
    
    PI_CC_SPIWriteReg(spi_parms, PI_CCxxx0_IOCFG2, 0x00); // GDO2 output pin config RX mode
}


// ------------------------------------------------------------------------------------------------
// Transmission of a block
void radio_send_block(spi_parms_t *spi_parms, uint8_t block_countdown)
// ------------------------------------------------------------------------------------------------
{
    uint8_t  initial_tx_count; // Number of bytes to send in first batch
    int      i, ret;

    radio_int_data.mode = RADIOMODE_TX;
    radio_int_data.packet_send = 0;
    radio_int_data.threshold_hits = 0;

    radio_set_packet_length(spi_parms, radio_int_data.tx_count);

    PI_CC_SPIWriteReg(spi_parms, PI_CCxxx0_IOCFG2,   0x02); // GDO2 output pin config TX mode

    // Initial number of bytes to put in FIFO is either the number of bytes to send or the FIFO size whichever is
    // the smallest. Actual size blocks you need to take size minus one byte.
    initial_tx_count = (radio_int_data.tx_count > PI_CCxxx0_FIFO_SIZE-1 ? PI_CCxxx0_FIFO_SIZE-1 : radio_int_data.tx_count);

    // Initial fill of TX FIFO
    PI_CC_SPIWriteBurstReg(spi_parms, PI_CCxxx0_TXFIFO, (uint8_t *) radio_int_data.tx_buf, initial_tx_count);
    radio_int_data.byte_index = initial_tx_count;
    radio_int_data.bytes_remaining = radio_int_data.tx_count - initial_tx_count;
    blocks_sent = radio_int_data.packet_tx_count;

    PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_STX); // Kick-off Tx

    while (blocks_sent == radio_int_data.packet_tx_count)
    {
        radio_wait_a_bit(4);
    }

    verbprintf(1, "Tx: packet #%d:%d\n", radio_int_data.packet_tx_count, block_countdown);
    print_block(4, (uint8_t *) radio_int_data.tx_buf, radio_int_data.tx_count);

    blocks_sent = radio_int_data.packet_tx_count;
    verbprintf(2,"Tx: packet length %d, FIFO threshold was hit %d times\n", radio_int_data.tx_count, radio_int_data.threshold_hits);
}

// ------------------------------------------------------------------------------------------------
// Transmission of a packet
void radio_send_packet(spi_parms_t *spi_parms, arguments_t *arguments, uint8_t *packet, uint32_t size)
// ------------------------------------------------------------------------------------------------
{
    int     block_countdown = size / (arguments->packet_length - 2);
    uint8_t *block_start = packet;
    uint8_t block_length;

    radio_int_data.tx_count = arguments->packet_length; // same block size for all

    while (block_countdown >= 0)
    {
        block_length = (size > arguments->packet_length - 2 ? arguments->packet_length - 2 : size);

        if (arguments->variable_length)
        {
            radio_int_data.tx_count = block_length + 2;
        }

        memset((uint8_t *) radio_int_data.tx_buf, 0, arguments->packet_length);
        memcpy((uint8_t *) &radio_int_data.tx_buf[2], block_start, block_length);
        radio_int_data.tx_buf[0] = block_length + 1; // size takes countdown counter into account
        radio_int_data.tx_buf[1] = (uint8_t) block_countdown; 

        radio_send_block(spi_parms, block_countdown);

        if (block_countdown > 0)
        {
            radio_wait_a_bit(arguments->packet_delay);
        }

        block_start += block_length;
        size -= block_length;
        block_countdown--;
    }

    packets_sent++;
}
*/