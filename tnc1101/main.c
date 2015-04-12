/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/*
/******************************************************************************/

#include <stdio.h>      // standard input / output functions
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <signal.h>

#include "main.h"
#include "util.h"
#include "serial.h"
#include "radio.h"
#include "msp430_interface.h"

arguments_t          arguments;
serial_t             serial_parms_usb, serial_parms_ax25;
msp430_radio_parms_t radio_parms;

char *tnc_mode_names[] = {
    "File bulk transmission",
    "File bulk reception",
    "KISS TNC",
    "SLIP TNC",
    "USB echo",
    "Radio status",
    "Radio init",
    "Radio block transmission test",
    "Radio block reception test",
    "Radio block echo test starting with Tx",
    "Radio block echo test starting with Rx",
    "Radio packet transmission test",
    "Radio packet reception test",
    "Radio packet reception test in non-blocking mode"
};

char *modulation_names[] = {
    "None",
    "OOK",
    "2-FSK",
    "4-FSK",
    "MSK",
    "GFSK",
};

uint32_t rate_values[] = {
    50,
    110,
    300,
    600,
    1200,
    2400,
    4800,
    9600,
    14400,
    19200,
    28800,
    38400,
    57600,
    76800,
    115200,
    250000,
    500000
};

uint8_t nb_preamble_bytes[] = {
    2,
    3,
    4,
    6,
    8,
    12,
    16,
    24
};

int power_values[] = {
    -30,
    -20,
    -15,
    -10,
    0,
    5,
    7,
    10
};

/***** Argp configuration start *****/

const char *argp_program_version = "TNC1101 0.1";
const char *argp_program_bug_address = "<f4exb06@gmail.com>";
static char doc[] = "TNC1101 -- TNC using CC1101 module and MSP430F5529 Launchpad for the radio link.";
static char args_doc[] = "";

static struct argp_option options[] = {
    {"verbose",  'v', "VERBOSITY_LEVEL", 0, "Verbosiity level: 0 quiet else verbose level (default : quiet)"},
    {"long-help",  'H', 0, 0, "Print a long help and exit"},
    {"real-time",  'T', 0, 0, "Engage so called \"real time\" scheduling (defalut 0: no)"},
    {"modulation",  'M', "MODULATION_SCHEME", 0, "Radio modulation scheme, See long help (-H) option"},
    {"rate",  'R', "DATA_RATE_INDEX", 0, "Data rate index, See long help (-H) option"},
    {"rate-skew",  'w', "RATE_MULTIPLIER", 0, "Data rate skew multiplier. (default 1.0 = no skew)"},
    {"block-delay",  'l', "DELAY_UNITS", 0, "Delay between successive radio blocks when transmitting a larger block in microseconds (default: 10000)"},
    {"modulation-index",  'm', "MODULATION_INDEX", 0, "Modulation index (default 0.5)"},
    {"fec",  'F', 0, 0, "Activate FEC (default off)"},
    {"whitening",  'W', 0, 0, "Activate whitening (default off)"},
    {"frequency",  'f', "FREQUENCY_HZ", 0, "Frequency in Hz (default: 433600000)"},
    {"offset-ppb",  'o', "FREQUENCY_OFFSET", 0, "Frequency offset in ppb from nominal (default: 0)"},
    {"if-frequency",  'I', "IF_FREQUENCY_HZ", 0, "Intermediate frequency in Hz (default: 310000)"},
    {"power-index",  'd', "POWER_INDEX", 0, "Power index, See long help (-H) option (default: 4 = 0dBm)"},
    {"packet-length",  'p', "PACKET_LENGTH", 0, "Packet length (fixed) or maximum packet length (variable) (default: 250)"},
    {"large-packet-length",  'P', "LARGE_PACKET_LENGTH", 0, "Large packet length (>255 bytes) for packet test only (default: 480)"},
    {"variable-length",  'V', 0, 0, "Variable packet length. Given packet length becomes maximum length (default off)"},
    {"tnc-mode",  't', "TNC_MODE", 0, "TNC mode of operation, See long help (-H) option fpr details (default : 0)"},
    {"test-phrase",  'y', "TEST_PHRASE", 0, "Set a test phrase to be used in test (default : \"Hello, World!\")"},
    {"repetition",  'n', "REPETITION", 0, "Repetiton factor wherever appropriate, see long Help (-H) option (default : 1 single)"},
    {"radio-status",  's', 0, 0, "Print radio status and exit"},
    {"tnc-usb-device",  'U', "USB_SERIAL_DEVICE", 0, "Hardware TNC USB device, (default : /dev/ttyACM2"},
    {"tnc-serial-device",  'D', "SERIAL_DEVICE", 0, "TNC Serial device, (default : /var/ax25/axp2)"},
    {"tnc-serial-speed",  'B', "SERIAL_SPEED", 0, "TNC Serial speed in Bauds (default : 9600)"},
    {"tnc-serial-window",  300, "TX_WINDOW_US", 0, "TNC time window in microseconds for concatenating serial frames. 0: no concatenation (default: 40ms))"},
    {"tnc-radio-window",  301, "RX_WINDOW_US", 0, "TNC time window in microseconds for concatenating radio frames. 0: no concatenation (default: 0))"},
    {"tnc-keyup-delay",  302, "KEYUP_DELAY_US", 0, "TNC keyup delay in microseconds (default: 10ms)."},
    {"tnc-keydown-delay",  303, "KEYDOWN_DELAY_US", 0, "FUTUR USE: TNC keydown delay in microseconds (default: 0 inactive)"},
    {"tnc-switchover-delay",  304, "SWITCHOVER_DELAY_US", 0, "FUTUR USE: TNC switchover delay in microseconds (default: 0 inactive)"},
    {"bulk-file",  310, "FILE_NAME", 0, "File name to send or receive with bulk transmission (default: '-' stdin or stdout"},
    {0}
};

// === Static functions declarations ==============================================================

static void delete_args(arguments_t *arguments);

static void file_bulk_transmit(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);

static void file_bulk_receive(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);

// === Static functions ===========================================================================

// ------------------------------------------------------------------------------------------------
// Terminator
static void terminate(const int signal_) {
// ------------------------------------------------------------------------------------------------
    printf("PICC: Terminating with signal %d\n", signal_);
    close_serial(&serial_parms_usb);
    close_serial(&serial_parms_ax25);
    delete_args(&arguments);
    exit(1);
}

// ------------------------------------------------------------------------------------------------
// Long help displays enumerated values
static void print_long_help()
{
    int i;

    fprintf(stderr, "Modulation scheme option -M values\n");
    fprintf(stderr, "Value:\tScheme:\n");

    for (i=0; i < RADIO_NUM_MOD; i++)
    {
        fprintf(stderr, "%2d\t%s\n", i, modulation_names[i]);
    }

    fprintf(stderr, "\nRate indexes option -R values\n");    
    fprintf(stderr, "Value:\tRate (Baud):\n");

    for (i=0; i<NUM_RATE; i++)
    {
        fprintf(stderr, "%2d\t%d\n", i, rate_values[i]);
    }

    fprintf(stderr, "\nOutput power option -d values\n");    
    fprintf(stderr, "Value:\tPower (dBm):\n");

    for (i=0; i<NUM_POWER; i++)
    {
        fprintf(stderr, "%2d\t%3d\n", i, power_values[i]);
    }

    fprintf(stderr, "\nTNC mode option -t values\n");
    fprintf(stderr, "Value:\tMode:\n");

    for (i=0; i<NUM_TNC; i++)
    {
        fprintf(stderr, "%2d\t%s\n", i, tnc_mode_names[i]);
    }

    fprintf(stderr, "\nRepetition factor option -n values\n");    
    fprintf(stderr, "- for test transmissions (-t option) this is the repetition of the same test packet\n");

}

// ------------------------------------------------------------------------------------------------
// Init arguments
static void init_args(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    arguments->verbose_level = 0;
    arguments->print_long_help = 0;
    arguments->usbacm_device = 0;
    arguments->serial_device = 0;
    arguments->bulk_filename = 0;
    arguments->serial_speed = B38400;
    arguments->serial_speed_n = 38400;
    arguments->print_radio_status = 0;
    arguments->modulation = RADIO_MOD_FSK2;
    arguments->rate = RATE_9600;
    arguments->rate_skew = 1.0;
    arguments->block_delay = 10000;
    arguments->modulation_index = 0.5;
    arguments->freq_offset_ppm = 0.0;
    arguments->power_index = 4;
    arguments->freq_hz = 433600000;
    arguments->if_freq_hz = 310000;
    arguments->packet_length = 250;
    arguments->large_packet_length = 480;
    arguments->variable_length = 0;
    arguments->tnc_mode = TNC_BULK_TX;
    arguments->test_phrase = strdup("Hello, World!");
    arguments->repetition = 1;
    arguments->fec = 0;
    arguments->whitening = 0;
    arguments->preamble = PREAMBLE_4;
    arguments->tnc_serial_window = 40000;
    arguments->tnc_radio_window = 0;
    arguments->tnc_keyup_delay = 4000;
    arguments->tnc_keydown_delay = 0;
    arguments->tnc_switchover_delay = 0;
    arguments->real_time = 0;
    arguments->slip = 0;
}

// ------------------------------------------------------------------------------------------------
// Delete arguments
void delete_args(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    if (arguments->serial_device)
    {
        free(arguments->serial_device);
    }
    if (arguments->usbacm_device)
    {
        free(arguments->usbacm_device);
    }
    if (arguments->test_phrase)
    {
        free(arguments->test_phrase);
    }
}

// ------------------------------------------------------------------------------------------------
// Bulk transmit the contents of a file (or stdin, or pipe)
static void file_bulk_transmit(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    FILE *fp;

    if (arguments->bulk_filename, "-") // not stdin
    {
        fp = fopen(arguments->bulk_filename, "r");
    }
    else // stdin
    {
        fp = stdin;
    }

    if (bulk_transmit(fp, serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "error transmitting %s\n", arguments->bulk_filename);
    }
    else
    {
        fprintf(stderr, "%s transmitted successfully\n", arguments->bulk_filename);
    }

    if (arguments->bulk_filename, "-") // not stdin
    {
        fclose(fp);
    }
}

// ------------------------------------------------------------------------------------------------
// Bulk receive to the contents of a file (or stdout, or pipe)
static void file_bulk_receive(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    FILE *fp;

    if (arguments->bulk_filename, "-") // not stdin
    {
        fp = fopen(arguments->bulk_filename, "w");
    }
    else // stdin
    {
        fp = stdout;
    }

    if (bulk_receive(fp, serial_parms, radio_parms, arguments))
    {
        fprintf(stderr, "error receiving %s\n", arguments->bulk_filename);
    }
    else
    {
        fprintf(stderr, "%s received successfully\n", arguments->bulk_filename);
    }

    if (arguments->bulk_filename, "-") // not stdout
    {
        fclose(fp);
    }
    else
    {
        fflush(fp);
    }
}

// ------------------------------------------------------------------------------------------------
// Print MFSK data
static void print_args(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
    fprintf(stderr, "-- options --\n");
    fprintf(stderr, "Verbosity ...........: %d\n", arguments->verbose_level);
    fprintf(stderr, "Real time ...........: %s\n", (arguments->real_time ? "yes" : "no"));
    fprintf(stderr, "--- radio ---\n");
    fprintf(stderr, "Modulation ..........: %s\n", modulation_names[arguments->modulation]);
    fprintf(stderr, "Rate nominal ........: %d Baud\n", rate_values[arguments->rate]);
    fprintf(stderr, "Rate skew ...........: %.2f\n", arguments->rate_skew);
    fprintf(stderr, "Block delay .........: %.2f ms\n", arguments->block_delay / 1000.0);
    fprintf(stderr, "Modulation index ....: %.2f\n", arguments->modulation_index);
    fprintf(stderr, "Frequency offset ....: %.2lf ppm\n", arguments->freq_offset_ppm);
    fprintf(stderr, "Frequency ...........: %d Hz\n", arguments->freq_hz);
    fprintf(stderr, "Output power ........: %d dBm\n", power_values[arguments->power_index]);
    fprintf(stderr, "Packet length .......: %d bytes\n", arguments->packet_length);
    fprintf(stderr, ">255 pkt len (test) .: %d bytes\n", arguments->large_packet_length);
    fprintf(stderr, "Variable length .....: %s\n", (arguments->variable_length ? "yes" : "no"));
    fprintf(stderr, "Preamble size .......: %d bytes\n", nb_preamble_bytes[arguments->preamble]);
    fprintf(stderr, "FEC .................: %s\n", (arguments->fec ? "on" : "off"));
    fprintf(stderr, "Whitening ...........: %s\n", (arguments->whitening ? "on" : "off"));
    fprintf(stderr, "TNC mode ............: %s\n", tnc_mode_names[arguments->tnc_mode]);
    fprintf(stderr, "--- test ---\n");
    fprintf(stderr, "Test phrase .........: %s\n", arguments->test_phrase);
    fprintf(stderr, "Test repetition .....: %d times\n", arguments->repetition);
    fprintf(stderr, "--- serial ---\n");
    fprintf(stderr, "Hardware TNC device .: %s\n", arguments->usbacm_device);
    fprintf(stderr, "TNC device ..........: %s\n", arguments->serial_device);
    fprintf(stderr, "TNC speed ...........: %d Baud\n", arguments->serial_speed_n);

    if (arguments->tnc_serial_window)
    {
        fprintf(stderr, "TNC serial window ...: %.2f ms\n", arguments->tnc_serial_window / 1000.0);
    }
    else
    {
        fprintf(stderr, "TNC serial window ...: none\n");   
    }

    if (arguments->tnc_radio_window)
    {
        fprintf(stderr, "TNC radio window ....: %.2f ms\n", arguments->tnc_radio_window / 1000.0);
    }
    else
    {
        fprintf(stderr, "TNC radio window ....: none\n");   
    }

    fprintf(stderr, "TNC keyup delay .....: %.2f ms\n", arguments->tnc_keyup_delay / 1000.0);
    fprintf(stderr, "TNC keydown delay ...: %.2f ms\n", arguments->tnc_keydown_delay / 1000.0);
    fprintf(stderr, "TNC switch delay ....: %.2f ms\n", arguments->tnc_switchover_delay / 1000.0);
    fprintf(stderr, "--- bulk transfer ---\n");
    fprintf(stderr, "Bulk filename .......: %s\n", arguments->bulk_filename);
}

// ------------------------------------------------------------------------------------------------
// Get TNC mode from index
static tnc_mode_t get_tnc_mode(uint8_t tnc_mode_index)
// ------------------------------------------------------------------------------------------------
{
    if (tnc_mode_index < NUM_TNC)
    {
        return (tnc_mode_t) tnc_mode_index;
    }
    else
    {
        return TNC_BULK_TX;
    }
}

// ------------------------------------------------------------------------------------------------
// Get modulation scheme from index
static radio_modulation_t get_modulation_scheme(uint8_t modulation_index)
// ------------------------------------------------------------------------------------------------
{
    if (modulation_index < RADIO_NUM_MOD)
    {
        return (radio_modulation_t) modulation_index;
    }
    else
    {
        return RADIO_MOD_FSK2;
    }
}

// ------------------------------------------------------------------------------------------------
// Get rate from index     
static rate_t get_rate(uint8_t rate_index)
// ------------------------------------------------------------------------------------------------
{
    if (rate_index < NUM_RATE)
    {
        return (rate_t) rate_index;
    }
    else
    {
        return RATE_9600;
    }
}

// ------------------------------------------------------------------------------------------------
// Get output power from index     
static rate_t get_power(uint8_t power_index)
// ------------------------------------------------------------------------------------------------
{
    if (power_index < NUM_POWER)
    {
        return (power_t) power_index;
    }
    else
    {
        return POWER_10;
    }
}

// ------------------------------------------------------------------------------------------------
// Option parser 
static error_t parse_opt (int key, char *arg, struct argp_state *state)
// ------------------------------------------------------------------------------------------------
{
    arguments_t *arguments = state->input;
    char        *end;  // Used to indicate if ASCII to int was successful
    uint8_t     i8;
    uint32_t    i32;

    switch (key){
        // Verbosity 
        case 'v':
            arguments->verbose_level = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            else
                verbose_level = arguments->verbose_level;
            break; 
        // Print long help and exit
        case 'H':
            arguments->print_long_help = 1;
            break;
        // Activate FEC
        case 'F':
            arguments->fec = 1;
            break;
        // Activate whitening
        case 'W':
            arguments->whitening = 1;
            break;
        // Reception test
        case 'r':
            arguments->test_rx = 1;
            break;
        // Modulation scheme 
        case 'M':
            i8 = strtol(arg, &end, 10); 
            if (*end)
                argp_usage(state);
            else
                arguments->modulation = get_modulation_scheme(i8);
            break;
        // TNC mode 
        case 't':
            i8 = strtol(arg, &end, 10); 
            if (*end)
                argp_usage(state);
            else
                arguments->tnc_mode = get_tnc_mode(i8);
            break;
        // Radio data rate 
        case 'R':
            i8 = strtol(arg, &end, 10); 
            if (*end)
                argp_usage(state);
            else
                arguments->rate = get_rate(i8);
            break;
        // Output power
        case 'd':
            i8 = strtol(arg, &end, 10); 
            if (*end)
                argp_usage(state);
            else
                arguments->power_index = get_power(i8);
            break;
        // Radio link frequency
        case 'f':
            arguments->freq_hz = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // Radio link intermediate frequency
        case 'I':
            arguments->if_freq_hz = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // Packet length
        case 'p':
            arguments->packet_length = strtol(arg, &end, 10) % 254;
            if (*end)
                argp_usage(state);
            break; 
        // Large packet length
        case 'P':
            arguments->large_packet_length = strtol(arg, &end, 10) % (1<<16);
            if (*end)
                argp_usage(state);
            break; 
        // Packet delay
        case 'l':
            arguments->block_delay = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // Variable length packet
        case 'V':
            if (ALLOW_VAR_BLOCKS)
            {
                arguments->variable_length = 1;
            }
            else
            {
                fprintf(stderr, "Variable length blocks are not allowed (yet?)\n");
            }
            break;
        // Real time scheduling
        case 'T':
            if (ALLOW_REAL_TIME)
            {
                arguments->real_time = 1;
            }
            else
            {
                fprintf(stderr, "Real time scheduling is not allowed\n");
            }
            break;
        // Repetition factor
        case 'n':
            arguments->repetition = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // USB device
        case 'U':
            arguments->usbacm_device = strdup(arg);
            break;
        // Serial device
        case 'D':
            arguments->serial_device = strdup(arg);
            break;
        // Serial speed  
        case 'B':
            i32 = strtol(arg, &end, 10); 
            if (*end)
                argp_usage(state);
            else
                arguments->serial_speed = get_serial_speed(i32, &(arguments->serial_speed_n));
            break;
        // Transmission test phrase
        case 'y':
            arguments->test_phrase = strdup(arg);
            break;
        // Print radio status and exit
        case 's':
            arguments->print_radio_status = 1;
            break;
        // Modulation index
        case 'm':
            arguments->modulation_index = atof(arg);
            break;
        // Frequency offset in ppb from nominal
        case 'o':
            arguments->freq_offset_ppm = atof(arg);
            break;
        // Rate skew multiplier
        case 'w':
            arguments->rate_skew = atof(arg);
            break;
        // TNC serial link window
        case 300:
            arguments->tnc_serial_window = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // TNC radio link window
        case 301:
            arguments->tnc_radio_window = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // TNC keyup delay
        case 302:
            arguments->tnc_keyup_delay = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // TNC keydown delay
        case 303:
            arguments->tnc_keydown_delay = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // TNC switchover delay 
        case 304:
            arguments->tnc_switchover_delay = strtol(arg, &end, 10);
            if (*end)
                argp_usage(state);
            break; 
        // Bkulk filename
        case 310:
            arguments->bulk_filename = strdup(arg);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------
static struct argp argp = {options, parse_opt, args_doc, doc};
// ------------------------------------------------------------------------------------------------

/***** ARGP configuration stop *****/

// ------------------------------------------------------------------------------------------------
int main (int argc, char **argv)
// ------------------------------------------------------------------------------------------------
{
    int i, nbytes;

    // unsolicited termination handling
    struct sigaction sa;
    // Catch all signals possible on process exit!
    for (i = 1; i < 64; i++) 
    {
        // skip SIGUSR2 for Wiring Pi
        if (i == 17)
            continue; 

        // These are uncatchable or harmless or we want a core dump (SEGV) 
        if (i != SIGKILL
            && i != SIGSEGV
            && i != SIGSTOP
            && i != SIGVTALRM
            && i != SIGWINCH
            && i != SIGPROF) 
        {
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = terminate;
            sigaction(i, &sa, NULL);
        }
    }

    // Set argument defaults
    init_args(&arguments); 

    // Parse arguments 
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    if (arguments.print_long_help)
    {
        print_long_help();
        return 0;
    }
    
    if (!arguments.usbacm_device)
    {
        arguments.usbacm_device = strdup("/dev/ttyACM2");
    }
    
    if (!arguments.serial_device)
    {
        arguments.serial_device = strdup("/var/ax25/axp2");
    }
    
    if (!arguments.bulk_filename)
    {
        arguments.bulk_filename = strdup("-");
    }

    set_serial_parameters(&serial_parms_ax25, arguments.serial_device, get_serial_speed(arguments.serial_speed, &arguments.serial_speed_n));
    set_serial_parameters(&serial_parms_usb,  arguments.usbacm_device, get_serial_speed(115200, &arguments.usb_speed_n));
    init_radio_parms(&radio_parms, &arguments);

    if (arguments.verbose_level > 0)
    {
        print_args(&arguments);
        print_radio_parms(&radio_parms);
        fprintf(stderr, "\n");
    }

    if (arguments.tnc_mode == TNC_KISS)
    {
        kiss_init(&arguments);
        kiss_run(&serial_parms_ax25, &serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_SLIP)
    {
        arguments.slip = 1;
        kiss_init(&arguments);
        kiss_run(&serial_parms_ax25, &serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_TEST_USB_ECHO) // This one does not need any access to the radio
    {
        usb_test_echo(&serial_parms_usb, &arguments);
    }
    else if (arguments.tnc_mode == TNC_RADIO_STATUS) 
    {
        print_radio_status(&serial_parms_usb, &arguments);
    }
    else if (arguments.tnc_mode == TNC_RADIO_INIT)
    {
        init_radio(&serial_parms_usb, &radio_parms, &arguments);

        if (nbytes < 0)
        {
            fprintf(stderr, "Error\n");
        }
    }
    else if (arguments.tnc_mode == TNC_TEST_TX)
    {
        radio_transmit_test(&serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_TEST_RX)
    {
        radio_receive_test(&serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_TEST_ECHO_TX)
    {
        radio_echo_test(&serial_parms_usb, &radio_parms, &arguments, 1);
    } 
    else if (arguments.tnc_mode == TNC_TEST_ECHO_RX)
    {
        radio_echo_test(&serial_parms_usb, &radio_parms, &arguments, 0);
    }
    else if (arguments.tnc_mode == TNC_TEST_TX_PACKET)
    {
        radio_packet_transmit_test(&serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_TEST_RX_PACKET)
    {
        radio_packet_receive_test(&serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_TEST_RX_PACKET_NON_BLOCKING)
    {
        radio_packet_receive_nb_test(&serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_BULK_TX)
    {
        file_bulk_transmit(&serial_parms_usb, &radio_parms, &arguments);
    }
    else if (arguments.tnc_mode == TNC_BULK_RX)
    {
        file_bulk_receive(&serial_parms_usb, &radio_parms, &arguments);
    }

    close_serial(&serial_parms_usb);
    close_serial(&serial_parms_ax25);
    delete_args(&arguments);
    return 0;
}
