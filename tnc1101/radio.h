/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* Radio link definitions                                                     */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#ifndef _RADIO_H_
#define _RADIO_H_

#include "msp430_interface.h"
#include "serial.h"

#define RADIO_BUFSIZE (1<<16)   // 256 max radio block size times a maximum of 256 radio blocs

typedef enum radio_int_scheme_e 
{
    RADIOINT_NONE = 0,   // Do not use interrupts
    RADIOINT_SIMPLE,     // Interrupts for packets fitting in FIFO
    RADIOINT_COMPOSITE,  // Interrupts for amy packet length up to 255
    NUM_RADIOINT
} radio_int_scheme_t;

typedef enum radio_mode_e
{
    RADIOMODE_NONE = 0,
    RADIOMODE_RX,
    RADIOMODE_TX,
    NUM_RADIOMODE
} radio_mode_t;

extern char     *modulation_names[];
extern char     *state_names[];
extern float    chanbw_limits[];
extern uint32_t packets_sent;
extern uint32_t packets_received;
extern uint32_t blocks_sent;
extern uint32_t blocks_received;

/*
void     init_radio_parms(radio_parms_t *radio_parms, arguments_t *arguments);
void     init_radio_int(spi_parms_t *spi_parms, arguments_t *arguments);
void     radio_init_rx(spi_parms_t *spi_parms, arguments_t *arguments);
void     radio_flush_fifos(spi_parms_t *spi_parms);

void     radio_turn_idle(spi_parms_t *spi_parms);
void     radio_turn_rx(spi_parms_t *spi_parms);

void     print_radio_parms(radio_parms_t *radio_parms);
*/
float   radio_get_byte_time(msp430_radio_parms_t *radio_parms);
float   radio_get_rate(msp430_radio_parms_t *radio_parms);

void    init_radio_parms(msp430_radio_parms_t *radio_parms, arguments_t *arguments);
int     init_radio(serial_t *serial_parms, msp430_radio_parms_t *radio_parms, arguments_t *arguments);
void    print_radio_status(serial_t *serial_parms, arguments_t *arguments);
int     radio_send_block(serial_t *serial_parms, 
            uint8_t  *dataBlock,
            uint8_t  dataSize,
            uint8_t  blockCountdown, 
            uint8_t  blockSize,
            uint8_t  *ackBlock, 
            int      *ackBlockSize, 
            uint32_t timeout_us);
void radio_send_packet(serial_t *serial_parms,
            uint8_t  *packet,
            uint8_t  dataBlockSize,
            uint32_t size,
            uint32_t block_timeout_us);
uint8_t radio_receive_block(serial_t *serial_parms, 
            uint8_t  *dataBlock, 
            uint8_t  dataBlockSize,
            uint32_t *size, 
            uint8_t  *rssi,
            uint8_t  *crc_lqi);
/*
int      radio_set_packet_length(spi_parms_t *spi_parms, uint8_t pkt_len);
uint8_t  radio_get_packet_length(spi_parms_t *spi_parms);
float    radio_get_rate(radio_parms_t *radio_parms);
float    radio_get_byte_time(radio_parms_t *radio_parms);
void     radio_wait_a_bit(uint32_t amount);
void     radio_wait_free();

void     radio_send_packet(spi_parms_t *spi_parms, arguments_t *arguments, uint8_t *packet, uint32_t size);
uint32_t radio_receive_packet(spi_parms_t *spi_parms, arguments_t *arguments, uint8_t *packet);
*/
#endif
