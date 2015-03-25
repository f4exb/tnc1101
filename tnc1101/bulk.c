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
    uint8_t  data_block_size = arguments->packet_length - 2;
    uint32_t blocks_per_buffer = (arguments->large_packet_length / data_block_size);
    uint32_t packet_time;
    uint8_t  *buffer, *xbuffer, *buffer0, *buffer1, *midbuffer;
    size_t   nbytes, nblocks, rbytes, ib;
    int      sbytes;

    int      ackbytes = 32;
    uint8_t  ackBlock[32];

    if (!init_radio(serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "Cannot initialize radio. Aborting...\n");
        return 1;
    }
    else
    {
        usleep(100000);
    }

    buffer = (uint8_t *) malloc(2 * (blocks_per_buffer+1) * data_block_size);
    
    midbuffer = buffer + (blocks_per_buffer+1) * data_block_size;
    xbuffer = buffer;    // store read bytes at this address
    buffer0 = buffer;    // current buffer
    buffer1 = midbuffer; // next buffer
    packet_time = ((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2);
    
    while ((nbytes = fread(xbuffer, sizeof(uint8_t), data_block_size * blocks_per_buffer, fp)) > 0)
    {
        verbprintf(2, "Read %d bytes from buffer at %x\n", nbytes, xbuffer);

        nblocks = nbytes / data_block_size;
        rbytes = nbytes % data_block_size;
        memcpy(buffer1, &buffer0[nblocks], rbytes);
        xbuffer = buffer1 + rbytes; // next read here

        radio_send_packet(serial_parms,
            buffer0,
            arguments->packet_length,
            nbytes,
            arguments->packet_delay,
            packet_time);

        /*
        for(ib = 0; ib < nblocks; ib++)
        {
            // send block
            sbytes = send_block(serial_parms, 
                &buffer0[ib*data_block_size],
                data_block_size, 
                arguments->packet_length,
                packet_time);

            if (sbytes <= 0)
            {
                verbprintf(1, "Error sending block. Aborting\n");
                free(buffer);
                return 1;
            }
        }
        */

        if (buffer1 > buffer0) // swap buffers
        {
            buffer1 = buffer;
            buffer0 = midbuffer;
        }
        else
        {
            buffer1 = midbuffer;
            buffer0 = buffer;
        }
        
    }

    if (rbytes > 0)
    {
        radio_send_packet(serial_parms,
            buffer1,
            arguments->packet_length,
            rbytes,
            arguments->packet_delay,
            packet_time);        
    }

    free(buffer);
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
            }

            break;
        }

        timeout = inter_packet_timeout;
    }

    /*
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
    */
    return 0;
}