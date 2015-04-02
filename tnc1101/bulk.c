/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* Bulk transmission                                                          */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "bulk.h"
#include "util.h"

// === Static functions declarations ==============================================================

// === Static functions ===========================================================================

// === Public functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Bulk transmit the contents of the file with file pointer fp.
int bulk_transmit(FILE *fp, 
    serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint8_t buffer[1<<16], ackBlock[32];
    int nbytes, ackbytes = 32, i;
    uint32_t block_time, bytes_left;

    block_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    memset(buffer, 0, (1<<16));
    i = 0;

    while ((nbytes = fread(buffer, sizeof(uint8_t), arguments->large_packet_length, fp)) > 0)
    //nbytes = arguments->large_packet_length;
    //for (i=0; i<arguments->repetition; i++)
    {
        verbprintf(2, "Packet #%d size %d\n", i, nbytes);

        bytes_left = radio_send_packet(serial_parms,
            buffer,
            arguments->packet_length,
            nbytes,
            arguments->packet_delay,
            block_time);

        if (bytes_left)
        {
            verbprintf(1, "Error in bulk transmission\n");
            return 1;
        }

        i++;
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------
// Bulk receive to the file with file pointer fp.
int bulk_receive(FILE *fp, 
    serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    uint32_t packets_received, size, block_time;
    uint8_t  dataBlock[1<<16];
    uint8_t  rssi, lqi, crc, crc_lqi, block_countdown;
    int      nbytes;
    uint32_t inter_packet_timeout = 500000, timeout = 4000000;

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Begin: cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    block_time = (((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2)) + arguments->packet_delay;

    while (1)
    {
        size = radio_receive_packet(serial_parms,
                    dataBlock,
                    arguments->packet_length,
                    timeout,
                    block_time);

        if (size > 0)
        {
            fwrite(dataBlock, sizeof(uint8_t), size, fp);
        }
        else // timeout or sever error so cancel Rx
        {
            nbytes = radio_cancel_rx(serial_parms);
            
            if (nbytes < 0)
            {
                verbprintf(1, "Cancel reception failed\n");

                if (!init_radio(serial_parms, radio_parms, arguments))
                {
                    fprintf(stderr, "End: cannot initialize radio (1).\n");
                }
                else
                {
                    usleep(100000);
                }

                if (!init_radio(serial_parms, radio_parms, arguments))
                {
                    fprintf(stderr, "End: cannot initialize radio (2).\n");
                }
                else
                {
                    usleep(100000);
                }
            }

            break;
        }

        timeout = inter_packet_timeout;
    }

    return 0;
}