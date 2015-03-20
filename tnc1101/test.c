/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* Test routines                                                              */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"
#include "radio.h"
#include "util.h"

// === Public functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Transmission test with interrupt handling
int radio_transmit_test(serial_t *serial_parms, msp430_radio_parms_t *radio_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_sent, packet_time;
    uint8_t  dataBlock[255], ackBlock[32];
    int nbytes, ackbytes = 32;
    
    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }

    memset(dataBlock, 0, 255);
    dataBlock[0] = arguments->packet_length;
    strncpy(&dataBlock[2], arguments->test_phrase, 252);
    packet_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);
    packets_sent = 0;

    verbprintf(0, "Sending %d test packets of size %d\n", arguments->repetition, arguments->packet_length);

    while (packets_sent < arguments->repetition)
    {
        nbytes = radio_send_block(serial_parms, arguments, dataBlock, ackBlock, &ackbytes, packet_time/4);
        verbprintf(2, "Packet #%d: %d bytes sent %d bytes received from radio_send_block\n", packets_sent, nbytes, ackbytes); 
        if (ackbytes > 0)
        {
            print_block(2, ackBlock, ackbytes);
        }
        packets_sent++;
    }

    return 0; 
}


// ------------------------------------------------------------------------------------------------
// Reception test with interrupt handlong
int radio_receive_test(serial_t *serial_parms, msp430_radio_parms_t *radio_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_received, packet_time;
    uint8_t  dataBlock[255];
    int      nbytes = 0, block_countdown;
    uint8_t  crc;

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }

    memset(dataBlock, 0, 255);
    packet_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);
    
    while (packets_received < arguments->repetition)
    {
        block_countdown = radio_receive_block(serial_parms, arguments, dataBlock, &nbytes, &crc, packet_time/4);
        verbprintf(2, "Packet #%d: Block countdown: %d BLock size: %d CRC: %s\n",
            packets_received, block_countdown, nbytes, (crc ? "OK" : "KO"));
        if (crc)
        {
            print_block(3, dataBlock, nbytes);
        }

        packets_received++;
    }    

    return 0;
}

/*
// ------------------------------------------------------------------------------------------------
// Reception test with interrupt handling
int radio_receive_test(spi_parms_t *spi_parms, arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint8_t nb_rx, rx_bytes[RADIO_BUFSIZE];

    init_radio_int(spi_parms, arguments);
    PI_CC_SPIStrobe(spi_parms, PI_CCxxx0_SFRX); // Flush Rx FIFO

    verbprintf(0, "Starting...\n");

    while((arguments->repetition == 0) || (packets_received < arguments->repetition))
    {
        radio_init_rx(spi_parms, arguments); // Init for new packet to receive
        radio_turn_rx(spi_parms);            // Put back into Rx

        do
        {
            radio_wait_free(); // make sure no radio operation is in progress
            nb_rx = radio_receive_packet(spi_parms, arguments, rx_bytes);
        } while(nb_rx == 0);

        rx_bytes[nb_rx] = '\0';
        verbprintf(0,"\"%s\"\n", rx_bytes);
    }
}

// ------------------------------------------------------------------------------------------------
// Simple echo test
void radio_test_echo(spi_parms_t *spi_parms, radio_parms_t *radio_parms, arguments_t *arguments, uint8_t active)
// ------------------------------------------------------------------------------------------------
{
    uint8_t  nb_bytes, rtx_bytes[RADIO_BUFSIZE];
    uint8_t  rtx_toggle, rtx_count;
    uint32_t timeout_value, timeout;
    struct timeval tdelay, tstart, tstop;

    init_radio_int(spi_parms, arguments);
    radio_flush_fifos(spi_parms);

    timeout_value = (uint32_t) (arguments->packet_length * 10 * radio_get_byte_time(radio_parms));
    timeout = 0;

    if (active)
    {
        nb_bytes = strlen(arguments->test_phrase);
        strcpy(rtx_bytes, arguments->test_phrase);
        rtx_toggle = 1;
    }
    else
    {
        rtx_toggle = 0;
    }

    while (packets_sent < arguments->repetition)
    {
        rtx_count = 0;

        do // Rx-Tx transaction in whichever order
        {
            if (arguments->tnc_keyup_delay)
            {
                usleep(arguments->tnc_keyup_delay);
            }

            if (rtx_toggle) // Tx
            {
                verbprintf(0, "Sending #%d\n", packets_sent);

                radio_wait_free(); // make sure no radio operation is in progress
                radio_send_packet(spi_parms, arguments, rtx_bytes, nb_bytes);
                radio_wait_a_bit(4);
                timeout = timeout_value; // arm Rx timeout
                rtx_count++;
                rtx_toggle = 0; // next is Rx
            }

            if (rtx_count >= 2)
            {
                break;
            }

            if (arguments->tnc_keydown_delay)
            {
                usleep(arguments->tnc_keydown_delay);
            }

            if (!rtx_toggle) // Rx
            {
                verbprintf(0, "Receiving #%d\n", packets_received);

                radio_init_rx(spi_parms, arguments); // Init for new packet to receive
                radio_turn_rx(spi_parms);            // Put back into Rx

                if (timeout > 0)
                {
                    gettimeofday(&tstart, NULL);
                }

                do
                {
                    radio_wait_free(); // make sure no radio operation is in progress
                    nb_bytes = radio_receive_packet(spi_parms, arguments, rtx_bytes);
                    radio_wait_a_bit(4);

                    if (timeout > 0)
                    {
                        gettimeofday(&tstop, NULL);
                        timeval_subtract(&tdelay, &tstop, &tstart);

                        if (ts_us(&tdelay) > timeout)
                        {
                            verbprintf(0, "Time out reached. Faking receiving data\n");
                            nb_bytes = strlen(arguments->test_phrase);
                            strcpy(rtx_bytes, arguments->test_phrase);                            
                            break;
                        }
                    }

                } while (nb_bytes == 0);

                rtx_count++;
                rtx_toggle = 1; // next is Tx                
            }

        } while(rtx_count < 2); 
    }    
}

*/