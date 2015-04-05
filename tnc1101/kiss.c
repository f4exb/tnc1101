/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* KISS AX.25 blocks handling                                                 */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#include <string.h>
#include <sys/time.h>

#include "kiss.h"
#include "radio.h"
#include "util.h"

static uint32_t tnc_tx_keyup_delay; // Tx keyup delay in microseconds
static float    kiss_persistence;   // Persistence parameter
static uint32_t kiss_slot_time;     // Slot time in microseconds
static uint32_t kiss_tx_tail;       // Tx tail in microseconds (obsolete)

// === Static functions declarations ==============================================================

static uint8_t *kiss_tok(uint8_t *block, uint8_t *end);
static uint8_t kiss_command(uint8_t *block);

// === Static functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Utility to unconcatenate KISS blocks. Returns pointer on next KISS delimiter past first byte (KISS_FEND = 0xC0)
// Assumes the pointer is currently on the opening KISS_FEND. Give pointer to first byte of block and past end pointer
uint8_t *kiss_tok(uint8_t *block, uint8_t *end)
// ------------------------------------------------------------------------------------------------
{
    uint8_t *p_cur, *p_ret = NULL;


    for (p_cur = block; p_cur < end; p_cur++)
    {
        if (p_cur == block)
        {
            if (*p_cur == KISS_FEND)
            {
                continue;
            }
            else
            {
                break; // will return NULL
            }
        }

        if (*p_cur == KISS_FEND)
        {
            p_ret = p_cur;
            break;
        }
    }

    return p_ret;
}

// ------------------------------------------------------------------------------------------------
// Check if the KISS block is a command block and interpret the command
// Returns 1 if this is a command block
// Returns 0 it this is a data block
uint8_t kiss_command(uint8_t *block)
// ------------------------------------------------------------------------------------------------
{
    uint8_t command_code = block[1] & 0x0F;
    uint8_t kiss_port = (block[1] & 0xF0)>>4;
    uint8_t command_arg = block[2];

    verbprintft(4, "KISS: command received for port %d: (%d,%d)\n", kiss_port, command_code, command_arg);

    switch (command_code)
    {
        case 0: // data block
            return 0;
        case 1: // TXDELAY
            tnc_tx_keyup_delay = command_arg * 10000; // these are tenths of ms
            verbprintft(1, ANSI_COLOR_GREEN "KISS command: change keyup delay to %d us" ANSI_COLOR_RESET "\n", tnc_tx_keyup_delay);
            break;
        case 2: // Persistence parameter
            kiss_persistence = (command_arg + 1) / 256.0;
            break;
        case 3: // Slot time
            kiss_slot_time = command_arg * 10000; // these are tenths of ms
            break;
        case 4: // Tx tail
            kiss_tx_tail = command_arg * 10000; // these are tenths of ms
            break;
        case 15:
            verbprintft(1, ANSI_COLOR_GREEN "KISS command: received aborting command" ANSI_COLOR_RESET "\n");
            abort();
            break;
        default:
            break;
    }

    return 1;
}

// === Public functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Initialize the common parameters to defaults
void kiss_init(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    tnc_tx_keyup_delay = arguments->tnc_keyup_delay; // 50ms Tx keyup delay
    kiss_persistence = 0.25;                          // 0.25 persistence parameter
    kiss_slot_time = 100000;                          // 100ms slot time
    kiss_tx_tail = 0;                                 // obsolete
}

// ------------------------------------------------------------------------------------------------
// Remove KISS signalling
void kiss_pack(uint8_t *kiss_block, uint8_t *packed_block, size_t *size)
// ------------------------------------------------------------------------------------------------
{
    size_t  new_size = 0, i;
    uint8_t fesc = 0;

    for (i=1; i<*size-1; i++)
    {
        if (kiss_block[i] == KISS_FESC) // FESC
        {
            fesc = 1;
            continue;
        }
        if (fesc)
        {
            if (kiss_block[i] == KISS_TFEND) // TFEND
            {
                packed_block[new_size++] = KISS_FEND; // FEND
            }
            else if (kiss_block[i] == KISS_TFESC) // TFESC
            {
                packed_block[new_size++] = KISS_FESC; // FESC
            }

             fesc = 0;
             continue;
        }

        packed_block[new_size++] = kiss_block[i];
    }

    *size = new_size;
}

// ------------------------------------------------------------------------------------------------
// Restore KISS signalling
void kiss_unpack(uint8_t *kiss_block, uint8_t *packed_block, size_t *size)
// ------------------------------------------------------------------------------------------------
{
    size_t  new_size = 0, i;

    kiss_block[0] = KISS_FEND; // FEND

    for (i=0; i<*size; i++)
    {
        if (packed_block[i] == KISS_FEND) // FEND
        {
            kiss_block[new_size++] = KISS_FESC; // FESC
            kiss_block[new_size++] = KISS_TFEND; // TFEND
        }
        else if (packed_block[i] == KISS_FESC) // FESC
        {
            kiss_block[new_size++] = KISS_FESC; // FESC
            kiss_block[new_size++] = KISS_TFESC; // TFESC
        }
        else
        {
            kiss_block[new_size++] = packed_block[i];
        }
    }

    kiss_block[new_size++] = KISS_FEND; // FEND
    *size = new_size;
}

// ------------------------------------------------------------------------------------------------
// Run the KISS virtual TNC
void kiss_run(serial_t *serial_parms_ax25,
    serial_t *serial_parms_usb,
    msp430_radio_parms_t *radio_parms,
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    static const size_t bufsize = (1<<16);
    uint8_t  rx_buffer[1<<16], tx_buffer[1<<16];
    uint8_t  rtx_tristate; // 0: no Rx/Tx operation, 1:Rx, 2:Tx
    uint8_t  rx_trigger, tx_trigger, force_mode;
    int      rx_count, tx_count, byte_count, nbytes;
    uint32_t timeout_value, bytes_left, block_time, block_delay;
    uint64_t timestamp;
    struct timeval tp;

    memset(rx_buffer, 0, bufsize);
    memset(tx_buffer, 0, bufsize);

    force_mode   = 0;
    rtx_tristate = 0;
    rx_trigger   = 0;
    tx_trigger   = 0;
    rx_count     = 0;
    tx_count     = 0;

    block_time  = (((uint32_t) radio_get_byte_time(radio_parms)) * (arguments->packet_length + 2)) + arguments->block_delay;
    block_delay = arguments->block_delay;

    if (!init_radio(serial_parms_usb, radio_parms, arguments))
    {
        verbprintft(1, ANSI_COLOR_RED "KISS run: cannot initialize radio. Aborting..." ANSI_COLOR_RESET "\n");
        return;
    }
    else
    {
        usleep(100000);
    }

    radio_turn_on_rx(serial_parms_usb, arguments->packet_length); // init for packet to receive

    verbprintft(1, ANSI_COLOR_YELLOW "KISS run: starting..." ANSI_COLOR_RESET "\n");

    while (1)
    {
        // Rx on CC1101 via USB

        byte_count = radio_receive_packet_nb(serial_parms_usb,
            &rx_buffer[rx_count],
            arguments->packet_length,
            1000,
            block_time);

        if (byte_count > 0) // Something received on radio
        {
            rx_count += byte_count;  // Accumulate Rx

            gettimeofday(&tp, NULL);
            timestamp = tp.tv_sec * 1000000ULL + tp.tv_usec;
            timeout_value = arguments->tnc_radio_window;
            force_mode = (timeout_value == 0);

            if (rtx_tristate == 2) // Tx to Rx transition
            {
                tx_trigger = 1; // Push Tx
            }
            else
            {
                tx_trigger = 0;
            }

            rtx_tristate = 1;
        }
        else if (byte_count < 0) // Error
        {
            verbprintft(1, ANSI_COLOR_RED "KISS receive USB: error in packet" ANSI_COLOR_RESET "\n");
            radio_turn_on_rx(serial_parms_usb, arguments->packet_length); // init for new packet to receive
            rtx_tristate = 0;
        }

        // Rx on AX.25 serial link

        byte_count = read_serial(serial_parms_ax25, &tx_buffer[tx_count], bufsize - tx_count);

        if (byte_count > 0) // something received on AX.25 serial
        {
            tx_count += byte_count;  // Accumulate Tx

            gettimeofday(&tp, NULL);
            timestamp = tp.tv_sec * 1000000ULL + tp.tv_usec;
            timeout_value = arguments->tnc_serial_window;
            force_mode = (timeout_value == 0);

            if (rtx_tristate == 1) // Rx to Tx transition
            {
                rx_trigger = 1;
            }
            else
            {
                rx_trigger = 0;
            }

            rtx_tristate = 2;
        }

        // Send bytes received from CC1101 radio link via USB on AX.25 serial

        if ((rx_count > 0) && ((rx_trigger) || (force_mode))) // Send bytes received on air to serial
        {
            verbprintft(2, ANSI_COLOR_YELLOW "KISS send AX.25: received %d bytes from radio" ANSI_COLOR_RESET "\n", rx_count);
            nbytes = write_serial(serial_parms_ax25, rx_buffer, rx_count);
            verbprintft(2, ANSI_COLOR_YELLOW "KISS send AX.25: sent %d bytes on AX.25 serial" ANSI_COLOR_RESET "\n", nbytes);
            memset(rx_buffer, 0, (1<<12)); // DEBUG
            rx_count = 0;
            rx_trigger = 0;
            force_mode = 0;
            rtx_tristate = 0;

            radio_turn_on_rx(serial_parms_usb, arguments->packet_length); // init for new packet to receive
        }

        // Send bytes received on AX.25 serial to CC1101 via USB for on air transmission

        if ((tx_count > 0) && ((tx_trigger) || (force_mode)))
        {
            print_block(4, tx_buffer, tx_count); // debug

            nbytes = radio_cancel_rx(serial_parms_usb);

            if (nbytes < 0)
            {
                verbprintft(1, ANSI_COLOR_RED "KISS send USB: cancel Rx failed. Aborting..." ANSI_COLOR_RESET "\n");
                return;
            }

            if (arguments->slip || !kiss_command(tx_buffer))
            {
                verbprintft(2, ANSI_COLOR_YELLOW "KISS send USB: %d bytes to send to radio" ANSI_COLOR_RESET "\n", tx_count);

                if (tnc_tx_keyup_delay)
                {
                    usleep(tnc_tx_keyup_delay);
                }

                bytes_left = radio_send_packet(serial_parms_usb,
                    tx_buffer,
                    arguments->packet_length,
                    tx_count,
                    block_delay,
                    block_time);

                if (bytes_left)
                {
                    verbprintft(1, ANSI_COLOR_RED "KISS send USB: error in packet transmission. Aborting..." ANSI_COLOR_RESET "\n");
                    return;
                }
            }

            memset(tx_buffer, 0, (1<<12)); // DEBUG
            tx_count = 0;
            tx_trigger = 0;
            force_mode = 0;
            rtx_tristate = 0;

            radio_turn_on_rx(serial_parms_usb, arguments->packet_length); // init for new packet to receive
        }

        // Time window processing

        if (rtx_tristate && !force_mode)
        {
            gettimeofday(&tp, NULL);

            if ((tp.tv_sec * 1000000ULL + tp.tv_usec) > timestamp + timeout_value)
            {
                force_mode = 1;
            }
            else
            {
                if (rtx_tristate == 1) // Rx going on
                {
                    radio_turn_on_rx(serial_parms_usb, arguments->packet_length); // init for new packet to receive
                }
            }
        }

        usleep(10);
    }
}
