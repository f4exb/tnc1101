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
#include "usbserial.h"
#include "serial.h"


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

	buffer[0] = (uint8_t) BLOCK_TYPE_ECHO_TEST;
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

// ------------------------------------------------------------------------------------------------
void usb_test_packet(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
	uint8_t buffer[260];
	int size;
	int usb_fd;

	usb_fd = usb_serial_open(arguments->usbacm_device);

	if (usb_fd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", arguments->usbacm_device);
		return;
	}

	usb_serial_setup(usb_fd, 115200);

	strcpy((char *) buffer, "45678");
	size = usb_serial_write(usb_fd, buffer, size);
	fprintf(stderr, "%d bytes written to USB\n", size);

    size = usb_serial_read(usb_fd, buffer, 260, 3);

	if (size > 0)
	{
		buffer[size] = '\0';
		printf("Returns: \"%s\"\n", (char *) buffer);
	}
	else
	{
		fprintf(stderr, "Error reading from %s\n", arguments->usbacm_device);
	}

	usb_serial_close(usb_fd);
}

// ------------------------------------------------------------------------------------------------
void usb_test_send(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
	uint8_t buffer[260];
	int size, rbytes;
	serial_t serial_parms;
	uint32_t timeout, speed;

	set_serial_parameters(&serial_parms, arguments->usbacm_device, get_serial_speed(115200, &speed));

	fprintf(stderr, "Start...\n");

	size = strlen(arguments->test_phrase);
	strcpy((char *) buffer, arguments->test_phrase);
	buffer[size] = '\r';

	size = write_serial(&serial_parms, buffer, size+1);
	fprintf(stderr, "%d bytes written to USB\n", size);

	size = 0;
	timeout = 100000;

	do
	{
    	rbytes = read_serial(&serial_parms, buffer, 260);
    	//usleep(10);
    	timeout--;
    } while ((rbytes <= 0) && (timeout > 0));

    printf("#1 %d %d\n", timeout, rbytes);

    if (rbytes > 0)
    {
		size += rbytes;
		buffer[size] = '\0';
		printf("Returns: \"%s\" (%d)\n", (char *) buffer, 100000 - timeout);
	}

	timeout = 100000;

	do
	{
    	rbytes = read_serial(&serial_parms, buffer+size, 260-size);
    	//usleep(10);
    	timeout--;
    } while ((rbytes <= 0) && (timeout > 0));

    printf("#2 %d %d\n", timeout, rbytes);

    if (rbytes > 0)
    {
		size += rbytes;
		buffer[size] = '\0';
		printf("Returns: \"%s\" (%d)\n", (char *) buffer, 100000 - timeout);
	}

	close_serial(&serial_parms);
}

/*
// ------------------------------------------------------------------------------------------------
void usb_test_send(arguments_t *arguments)
// ------------------------------------------------------------------------------------------------
{
	uint8_t buffer[260];
	int size;
	int usb_fd;

	usb_fd = usb_serial_open(arguments->usbacm_device);

	if (usb_fd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", arguments->usbacm_device);
		return;
	}

	usb_serial_setup(usb_fd, 1000000);

	size = strlen(arguments->test_phrase);
	strcpy((char *) buffer, arguments->test_phrase);
	buffer[size] = '\r';

	size = usb_serial_write(usb_fd, buffer, size+1);
	fprintf(stderr, "%d bytes written to USB\n", size);

    size = usb_serial_read(usb_fd, buffer, 260, 3);

	if (size > 0)
	{
		buffer[size] = '\0';
		printf("Returns: \"%s\"\n", (char *) buffer);
	}
	else
	{
		fprintf(stderr, "Error reading from %s\n", arguments->usbacm_device);
	}

	usb_serial_close(usb_fd);
}
*/
