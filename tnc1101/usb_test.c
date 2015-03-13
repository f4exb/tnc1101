/******************************************************************************/
/* TNC1101  - Radio serial link using CC1101 module                           */
/*                                                                            */
/* Test USB link with the MSP430F5529 Launchpad                               */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "usb_test.h" 
#include "serial.h"
#include "msp430_interface.h"

// ------------------------------------------------------------------------------------------------
void usb_test_echo(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
	uint8_t buffer[260];
	int size, rbytes;
	serial_t serial_parms;
	uint32_t timeout, speed;

	set_serial_parameters(&serial_parms, arguments->usbacm_device, get_serial_speed(115200, &speed));

	fprintf(stderr, "Start...\n");

	buffer[0] = (uint8_t) MSP430_BLOCK_TYPE_ECHO_TEST;
	buffer[1] = strlen(arguments->test_phrase);
	strncpy((char *) &buffer[2], arguments->test_phrase, 254);
	size = buffer[1] + 2;

	rbytes = write_serial(&serial_parms, buffer, size);
	fprintf(stderr, "%d bytes written to USB\n", rbytes);

	timeout = 100000;

	do
	{
		rbytes = read_serial(&serial_parms, buffer, 260);
		usleep(10);
		timeout--;
	} while ((rbytes <= 0) && (timeout > 0));

	if (rbytes > 0)
	{
		buffer[rbytes] = '\0';
		printf("Returns: \"%s\"\n", (char *) &buffer[2]);
	}
	else
	{
		fprintf(stderr, "Error reading from %s\n", arguments->usbacm_device);
	}

	close_serial(&serial_parms);
}

