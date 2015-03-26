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

static int send_block(serial_t *serial_parms,
    uint8_t  *buffer,
    uint8_t  data_size,
    uint8_t  block_size,
    uint32_t timeout_us);

// === Static functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Send a block of data over the radio link
int send_block(serial_t *serial_parms,
    uint8_t  *buffer,
    uint8_t  data_size,
    uint8_t  block_size,
    uint32_t timeout_us)
// ------------------------------------------------------------------------------------------------
{
    int      nbytes, ackbytes = 32;
    uint8_t  ackBlock[32];

    // send block
    nbytes = radio_send_block(serial_parms, 
        buffer,
        data_size + 1, // + block countdown
        0, // block countdown of zero for a single block packet
        block_size,    // radio (fixed) block size
        ackBlock, 
        &ackbytes, 
        timeout_us); // 10's of microseconds. 10 times provision.

    verbprintf(2, "%d bytes sent %d bytes received from radio_send_block\n", 
        nbytes, 
        ackbytes); 
    
    if (ackbytes > 0)
    {
        print_block(2, ackBlock, ackbytes);
    }      

    return ackbytes;          
}
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
    uint32_t block_time;

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
    //for (i=0; i<arguments->repetition; i++)
    {
        verbprintf(2, "Packet #%d size %d\n", i, nbytes); 

        radio_send_packet(serial_parms,
            buffer,
            arguments->packet_length,
            nbytes,
            arguments->packet_delay,
            block_time);

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