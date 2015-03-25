/******************************************************************************/
/* PiCC1101  - Radio serial link using CC1101 module and Raspberry-Pi         */
/*                                                                            */
/* Bulk transmission                                                          */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#ifndef _BULK_H_
#define _BULK_H_

#include <stdio.h>

#include "main.h"
#include "radio.h"


int bulk_transmit(FILE *fp, 
    serial_t *serial_parms, 
    msp430_radio_parms_t *radio_parms, 
    arguments_t *arguments);

#endif // _BULK_H_