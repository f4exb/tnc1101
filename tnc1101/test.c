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

    dataBlock[0] = strlen(arguments->test_phrase) + 1; // + block countdown
    dataBlock[1] = 0; // block countdown of zero for a single block packet
    
    strncpy(&dataBlock[2], arguments->test_phrase, arguments->packet_length-2); // - count - block countdown  
    
    packet_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);
    
    packets_sent = 0;

    verbprintf(0, "Sending %d test packets of size %d\n", 
        arguments->repetition, 
        arguments->packet_length);

    while (packets_sent < arguments->repetition)
    {
        nbytes = radio_send_block(serial_parms, 
            dataBlock, 
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
// Reception test with interrupt handlong
int radio_receive_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_received;
    uint8_t  dataBlock[255];
    char     displayBlock[255];
    int      nbytes, block_countdown;
    uint8_t  rssi, lqi, crc, crc_lqi;

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
        nbytes = 0;

        block_countdown = radio_receive_block(serial_parms, 
            dataBlock,
            arguments->packet_length, 
            &nbytes,
            &rssi, 
            &crc_lqi);
        
        crc = get_crc_lqi(crc_lqi, &lqi);

        verbprintf(1, "Packet #%d: Block countdown: %d Data size: %d RSSI: %.1f dBm CRC: %s\n",
            packets_received, 
            block_countdown, 
            nbytes, 
            rssi_dbm(rssi),
            (crc ? "OK" : "KO"));

        if (crc)
        {
            strncpy(displayBlock, dataBlock, nbytes);
            displayBlock[nbytes] = '\0';
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
    uint32_t packet_time, packets_sent, packets_received;
    uint8_t  rtx_toggle, rtx_count;
    uint8_t  dataBlock[255], displayBlock[255], ackBlock[32];
    int      nbytes, block_countdown, ackbytes = 32;
    uint8_t  rssi, lqi, crc, crc_lqi;

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

    if (active)
    {
        dataBlock[0] = strlen(arguments->test_phrase) + 1; // + block countdown
        dataBlock[1] = 0; // block countdown of zero for a single block packet
        strncpy(&dataBlock[2], arguments->test_phrase, 253);
        rtx_toggle = 1;
    }
    else
    {
        rtx_toggle = 0;
    }

    verbprintf(0, "Starting echo test with %d test packets of size %d. Begin with %s...\n", 
        arguments->repetition, 
        arguments->packet_length,
        (rtx_toggle ? "Tx" : "Rx"));

    packet_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);    
    packets_sent = 0;
    packets_received = 0;

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
                    arguments->packet_length, 
                    ackBlock, 
                    &ackbytes, 
                    packet_time/4);

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

                nbytes = 0;

                block_countdown = radio_receive_block(serial_parms, 
                    &dataBlock[2],
                    arguments->packet_length, 
                    &nbytes,
                    &rssi, 
                    &crc_lqi);
                
                crc = get_crc_lqi(crc_lqi, &lqi);

                verbprintf(1, "Packet #%d: Block countdown: %d Data size: %d RSSI: %.1f dBm CRC: %s\n",
                    packets_received, 
                    block_countdown, 
                    nbytes, 
                    rssi_dbm(rssi),
                    (crc ? "OK" : "KO"));

                if (crc)
                {
                    strncpy(displayBlock, &dataBlock[2], nbytes);
                    displayBlock[nbytes] = '\0';
                    verbprintf(1, ">%s\n", displayBlock);
                }

                packets_received++;
                rtx_count++;
                rtx_toggle = 1; // next is Tx                
            }

        } while(rtx_count < 2); 
    }

    return 0;  
}

