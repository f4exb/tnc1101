/******************************************************************************/
/* TNC1101  - Radio serial link using CC1101 module                           */
/*                                                                            */
/* Test USB link with the MSP430F5529 Launchpad                               */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#ifndef _USB_TEST_H
#define _USB_TEST_H

#include "main.h"
#include "serial.h"

void usb_test_echo(serial_t *serial_parms, arguments_t *arguments);

#endif