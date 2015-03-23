/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* Test routines                                                              */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#ifndef _TEST_H_
#define _TEST_H_

#include "main.h"
#include "serial.h"
#include "radio.h"

int radio_transmit_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);

int radio_receive_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);

int radio_echo_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments, 
    uint8_t active);

int radio_packet_transmit_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);

int radio_packet_receive_test(serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);


#endif