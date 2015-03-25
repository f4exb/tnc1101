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
// Transmission test with (<255 bytes) blocks
int radio_transmit_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
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
    else
    {
        usleep(100000);
    }

    memset(dataBlock, 0, 255);

    strncpy(dataBlock, arguments->test_phrase, arguments->packet_length-2); // - count - block countdown  
    
    packet_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);
    
    packets_sent = 0;

    verbprintf(0, "Sending %d test packets of size %d\n", 
        arguments->repetition, 
        arguments->packet_length);

    while (packets_sent < arguments->repetition)
    {
        nbytes = radio_send_block(serial_parms, 
            dataBlock,
            strlen(arguments->test_phrase) + 1, // + block countdown
            0, // block countdown of zero for a single block packet
            arguments->packet_length, 
            ackBlock, 
            &ackbytes, 
            packet_time/4);

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
// Transmission test with (large >255 bytes) packets
int radio_packet_transmit_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_sent, block_time, block_delay;
    uint8_t  dataBlock[1<<16];

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    memset(dataBlock, 0, 1<<16);
    strncpy(dataBlock, arguments->test_phrase, arguments->large_packet_length);

    block_time  = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);
    block_delay = arguments->packet_delay;

    packets_sent = 0;

    verbprintf(0, "Sending %d test packets of size %d\n", 
        arguments->repetition, 
        arguments->large_packet_length);

    while (packets_sent < arguments->repetition)
    {
        verbprintf(1, "Packet #%d\n", packets_sent);

        radio_send_packet(serial_parms,
            dataBlock,
            arguments->packet_length,
            arguments->large_packet_length,
            block_delay,
            block_time);

        packets_sent++;
    }
}

// ------------------------------------------------------------------------------------------------
// Reception test with interrupt handlong
int radio_receive_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_received, size;
    uint8_t  dataBlock[255];
    char     displayBlock[255];
    uint8_t  rssi, lqi, crc, crc_lqi, block_countdown;
    int      nbytes;

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    memset(dataBlock, 0, 255);
    
    verbprintf(0, "Receiving %d test packets of size %d\n", 
        arguments->repetition, 
        arguments->packet_length);

    while (packets_received < arguments->repetition)
    {
        size = 0;

        nbytes = radio_receive_block(serial_parms, 
            dataBlock,
            arguments->packet_length,
            &block_countdown,
            &size,
            &rssi, 
            &crc_lqi,
            0);

        if (nbytes > 4)
        {
            crc = get_crc_lqi(crc_lqi, &lqi);

            verbprintf(1, "Packet #%d: Block countdown: %d Data size: %d RSSI: %.1f dBm CRC: %s\n",
                packets_received, 
                block_countdown, 
                size, 
                rssi_dbm(rssi),
                (crc ? "OK" : "KO"));

            if (crc)
            {
                strncpy(displayBlock, dataBlock, size);
                displayBlock[size] = '\0';
                verbprintf(1, ">%s\n", displayBlock);
            }
        }
        else
        {
            verbprintf(1, "Packet #%d: not received or incomplete\n");
        }

        packets_received++;
    }    

    return 0;
}

// ------------------------------------------------------------------------------------------------
// Reception test with (large >255 bytes) packets
int radio_packet_receive_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_received, size, block_time;
    uint8_t  dataBlock[1<<16];
    char     displayBlock[1<<8];
    uint8_t  rssi, lqi, crc, crc_lqi, block_countdown;
    int      nbytes;

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    block_time = (((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2)) + arguments->packet_delay;

    verbprintf(0, "Receiving %d test packets with radio block size %d\n", 
        arguments->repetition, 
        arguments->packet_length);

    while (packets_received < arguments->repetition)
    {
        dataBlock[0] = '\0';
        verbprintf(1, "Packet #%d\n", packets_received);

        size = radio_receive_packet(serial_parms,
                    dataBlock,
                    arguments->packet_length,
                    0,
                    block_time);

        if (size > 0)
        {
            strncpy(displayBlock, dataBlock, 255);
            verbprintf(1, ">%s\n", displayBlock);
        }

        packets_received++;
    }

    return 0;    
}

// ------------------------------------------------------------------------------------------------
// Simple echo test
// If active = 1 then start with Tx else start with Rx
int radio_echo_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments, 
    uint8_t active)
// ------------------------------------------------------------------------------------------------
{
    uint32_t block_time, block_delay, packets_sent, packets_received, size, rx_timeout;
    uint8_t  rtx_toggle, rtx_count;
    uint8_t  dataBlock[255], displayBlock[255], ackBlock[32];
    int      nbytes, ackbytes = 32;
    uint8_t  rssi, lqi, crc, crc_lqi, block_countdown;
    uint8_t  data_size;

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    memset(dataBlock, 0, 255);    
    block_time  = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);    
    block_delay = ((uint32_t) radio_get_byte_time(radio_parms)) * arguments->packet_delay;    
    packets_sent = 0;
    packets_received = 0;

    if (active)
    {
        data_size = strlen(arguments->test_phrase) + 1; // + block countdown
        strncpy(dataBlock, arguments->test_phrase, 253);
        rtx_toggle = 1;
        rx_timeout = block_time + arguments->tnc_keyup_delay;
    }
    else
    {
        rtx_toggle = 0;
        rx_timeout = 0;
    }

    verbprintf(0, "Starting echo test with %d test packets of size %d. Begin with %s...\n", 
        arguments->repetition, 
        arguments->packet_length,
        (rtx_toggle ? "Tx" : "Rx"));

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

                nbytes = radio_send_block(serial_parms, 
                    dataBlock,
                    data_size,
                    0,
                    arguments->packet_length, 
                    ackBlock, 
                    &ackbytes, 
                    block_time/4);

                verbprintf(2, "Packet #%d: %d bytes sent %d bytes received from radio_send_block\n", 
                    packets_sent, 
                    nbytes, 
                    ackbytes); 
                
                if (ackbytes > 0)
                {
                    print_block(3, ackBlock, ackbytes);
                }

                packets_sent++;
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

                size = 0;

                nbytes = radio_receive_block(serial_parms, 
                    dataBlock,
                    arguments->packet_length,
                    &block_countdown, 
                    &size,
                    &rssi, 
                    &crc_lqi,
                    rx_timeout/4);
                
                if (nbytes > 4)
                {
                    crc = get_crc_lqi(crc_lqi, &lqi);

                    verbprintf(1, "Packet #%d: Block countdown: %d Data size: %d RSSI: %.1f dBm CRC: %s\n",
                        packets_received, 
                        block_countdown, 
                        size, 
                        rssi_dbm(rssi),
                        (crc ? "OK" : "KO"));

                    if (crc)
                    {
                        strncpy(displayBlock, dataBlock, size);
                        displayBlock[size] = '\0';
                        verbprintf(1, ">%s\n", displayBlock);
                        data_size = size + 1; // + block countdown
                    }
                    else
                    {
                        data_size = 1;
                    }
                }
                else
                {
                    verbprintf(1, "Packet #%d: Timeout or block inmcomplete\n", packets_received);
                }

                rx_timeout = block_time + arguments->tnc_keyup_delay;
                packets_received++;
                rtx_count++;
                rtx_toggle = 1; // next is Tx                
            }

        } while(rtx_count < 2); 
    }

    return 0;  
}

