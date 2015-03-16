/* --COPYRIGHT--,BSD
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/*  
 * ======== main.c ========
 * Local Echo Demo:
 *
 * This example simply echoes back characters it receives from the host.  
 * Unless the terminal application has a built-in echo feature turned on, 
 * typing characters into it only causes them to be sent; not displayed locally.
 * This application causes typing in Hyperterminal to feel like typing into any 
 * other PC application – characters get displayed.  
 *
 * ----------------------------------------------------------------------------+
 * Please refer to the Examples Guide for more details.
 * ---------------------------------------------------------------------------*/
#include <string.h>

#include "driverlib.h"

#include "USB_config/descriptors.h"
#include "USB_API/USB_Common/device.h"
#include "USB_API/USB_Common/usb.h"                 // USB-specific functions
#include "USB_API/USB_CDC_API/UsbCdc.h"
#include "USB_app/usbConstructs.h"

/*
 * NOTE: Modify hal.h to select a specific evaluation board and customize for
 * your own board.
 */
#include "hal.h"
#include "util.h"
#include "radio.h"
#include "msp430_interface.h"
#include "TI_CC_hardware_board.h"

// Global flags set by events
volatile uint8_t bCDCDataReceived_event = FALSE;  // Flag set by event handler to 
                                               // indicate data has been 
                                               // received into USB buffer

#define BUFFER_SIZE 260                // Command + size + data (size + block countdown + data + RSSI + LQI)
                                       //       1 +    1         ------------------------- 256 +    1 +   1 
char    dataBuffer[BUFFER_SIZE] = "";  // Current I/O buffer
char    outString[65];                 // Holds outgoing strings to be sent
uint8_t send_ack = 0;                  // Set when an ack is to be sent

// = Static functions declarations =================================================================

static void    init_leds();
static void    init_left_button(); 
static void    init_gdo_int(); 
static void    set_red_led(uint8_t on);
static void    set_green_led(uint8_t on);
static void    toggle_red_led();
static void    toggle_green_led();
static uint8_t process_usb_block(uint16_t count, uint8_t *block);

// = Static functions =============================================================================

// ------------------------------------------------------------------------------------------------
// Init the board LEDs
void init_leds()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_RED_LED_PxDIR   = TI_CC_RED_LED;
    TI_CC_GREEN_LED_PxDIR = TI_CC_GREEN_LED;
}

// ------------------------------------------------------------------------------------------------
// Init the board left button with interrupt handler
void init_left_button()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_SWL_PxREN |= TI_CC_SWL;  // Enable pull-up and pull-down
    TI_CC_SWL_PxOUT = TI_CC_SWL;   // Pull-up
    TI_CC_SWL_PxIE |= TI_CC_SWL;   // Interrupt enabled
    TI_CC_SWL_PxIES |= TI_CC_SWL;  // Hi/lo falling edge
    TI_CC_SWL_PxIFG &= ~TI_CC_SWL; // IFG cleared just in case
}

// ------------------------------------------------------------------------------------------------
// Set the red led on or off
void set_red_led(uint8_t on)
// ------------------------------------------------------------------------------------------------
{
    if (on)
    {
        TI_CC_RED_LED_PxOUT |= TI_CC_RED_LED;
    }
    else
    {
        TI_CC_RED_LED_PxOUT &= ~TI_CC_RED_LED;        
    }
}

// ------------------------------------------------------------------------------------------------
// Set the green led on or off
void set_green_led(uint8_t on)
// ------------------------------------------------------------------------------------------------
{
    if (on)
    {
        TI_CC_GREEN_LED_PxOUT |= TI_CC_GREEN_LED;
    }
    else
    {
        TI_CC_GREEN_LED_PxOUT &= ~TI_CC_GREEN_LED;        
    }
}

// ------------------------------------------------------------------------------------------------
// Toggle the red led
void toggle_red_led()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_RED_LED_PxOUT ^= TI_CC_RED_LED;
}

// ------------------------------------------------------------------------------------------------
// Toggle the red led
void toggle_green_led()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_GREEN_LED_PxOUT ^= TI_CC_GREEN_LED;
}

// ------------------------------------------------------------------------------------------------
// Process an incoming USB block
uint8_t process_usb_block(uint16_t count, uint8_t *dataBuffer)
// ------------------------------------------------------------------------------------------------
{
    uint8_t byte_count = dataBuffer[1];
    char str_byte[4];

    if (dataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_ECHO_TEST)
    {
        strcpy(outString, "xxEcho Command - Byte count: ");
        print_byte_decimal(byte_count, str_byte);
        strcat(outString, str_byte);                        

        // Count has the number of bytes received into dataBuffer
        // Echo back to the host.
        if (cdcSendDataInBackground((uint8_t *) outString, strlen(outString), CDC0_INTFNUM, 1))
        {
            // Exit if something went wrong.
            return 1;
        }
    }
    else if (dataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_RADIO_STATUS)
    {
        get_radio_status(&dataBuffer[2]);
        dataBuffer[1] = TI_CCxxx0_NUM_STATUS;

        if (cdcSendDataInBackground((uint8_t *) dataBuffer, TI_CCxxx0_NUM_STATUS + 2, CDC0_INTFNUM, 1))
        {
            // Exit if something went wrong.
            return 1;
        }
    }
    else if (dataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_INIT)
    {
        init_radio((msp430_radio_parms_t *) &dataBuffer[2]);
        dataBuffer[1] = 0;

        if (cdcSendDataInBackground((uint8_t *) dataBuffer, 2, CDC0_INTFNUM, 1))
        {
            // Exit if something went wrong.
            return 1;
        }
    }

    return 0;
}

// = Interrupt handlers ===========================================================================

// ------------------------------------------------------------------------------------------------
// Port 2 interrupt service routine
// Left button on P2.1
// GDO2        on P2.4
// GDO0        on P2.5
#if defined(__TI_COMPILER_VERSION__) || (__IAR_SYSTEMS_ICC__)
#pragma vector = PORT2_VECTOR
__interrupt void PORT2_ISR (void)
#elif defined(__GNUC__) && (__MSP430__)
void __attribute__ ((interrupt(PORT2_VECTOR))) PORT2_ISR (void)
#else
#error Compiler not found!
#endif
// ------------------------------------------------------------------------------------------------
{
    __disable_interrupt();

    if (TI_CC_SWL_PxIFG & TI_CC_SWL) // Left button
    {
        toggle_red_led();
        TI_CC_SWL_PxIFG &= ~TI_CC_SWL; // Clear IFG
    }

    __enable_interrupt();  // Enable interrupts globally
}

// = Main =========================================================================================

// ------------------------------------------------------------------------------------------------
// Main routine
void main (void)
// ------------------------------------------------------------------------------------------------
{
    WDT_A_hold(WDT_A_BASE); // Stop watchdog timer
    __disable_interrupt();

    // Minimum Vcore setting required for the USB API is PMM_CORE_LEVEL_2 .
#ifndef DRIVERLIB_LEGACY_MODE
    PMM_setVCore(PMM_CORE_LEVEL_2);
#else
    PMM_setVCore(PMM_BASE, PMM_CORE_LEVEL_2);
#endif

    initPorts();           // Config GPIOS for low-power (output low)
    initClocks(8000000);   // Config clocks. MCLK=SMCLK=FLL=8MHz; ACLK=REFO=32kHz
    USB_setup(TRUE, TRUE); // Init USB & events; if a host is present, connect

    __delay_cycles(5000);  // 5ms delay to compensate for time to startup between MSP430 and CC1100/2500 
    init_radio_spi();      // Initialize SPI comm with radio module
    init_leds();
    init_left_button();

    __bis_SR_register(LPM0_bits + GIE); // Enter LPM0 until awakened by an event handler

    __enable_interrupt();  // Enable interrupts globally

    while (1)
    {
        //uint8_t ReceiveError = 0, SendError = 0;
        uint8_t  retVal = 0;
        uint16_t count;
        //uint8_t byte_command;
        //uint8_t byte_count;
        //char str_byte[4];
        
        // Check the USB state and directly main loop accordingly
        switch (USB_connectionState())
        {
            // This case is executed while your device is enumerated on the
            // USB host
            case ST_ENUM_ACTIVE:
                // You will not want this in the general case
                // Sleep if there are no bytes to process.
                /*
                __disable_interrupt();
                if (!USBCDC_bytesInUSBBuffer(CDC0_INTFNUM)) {
                
                    // Enter LPM0 until awakened by an event handler
                    __bis_SR_register(LPM0_bits + GIE);
                }

                __enable_interrupt();
                */
                // Exit LPM because of a data-receive event, and
                // fetch the received data
                if (bCDCDataReceived_event){
                
                    // Clear flag early -- just in case execution breaks
                    // below because of an error
                    bCDCDataReceived_event = FALSE;

                    count = cdcReceiveDataInBuffer((uint8_t*) dataBuffer, BUFFER_SIZE, CDC0_INTFNUM);

                    if (count >= 2)
                    {
                        retVal = process_usb_block(count, (uint8_t*) dataBuffer);

                        if (retVal)
                        {
                            break;
                        }
                        /*
                        byte_command = dataBuffer[0];
                        byte_count = dataBuffer[1];

                        strcpy(outString, "xxCommand: ");
                        print_byte_decimal(byte_command, str_byte);
                        strcat(outString, str_byte);
                        strcat(outString, " - Byte count: ");
                        print_byte_decimal(byte_count, str_byte);
                        strcat(outString, str_byte);                        

                        // Count has the number of bytes received into dataBuffer
                        // Echo back to the host.
                        if (cdcSendDataInBackground((uint8_t *) outString, strlen(outString), CDC0_INTFNUM, 1))
                        {
                            // Exit if something went wrong.
                            SendError = 0x01;
                            break;
                        }
                        */
                    }
                }

                __bis_SR_register(LPM0_bits + GIE); // Enter LPM0 until awakened by an event handler
                break; // ST_ENUM_ACTIVE
                
            // These cases are executed while your device is disconnected from
            // the host (meaning, not enumerated); enumerated but suspended
            // by the host, or connected to a powered hub without a USB host
            // present.
            case ST_PHYS_DISCONNECTED:
            case ST_ENUM_SUSPENDED:
            case ST_PHYS_CONNECTED_NOENUM_SUSP:
                __bis_SR_register(LPM3_bits + GIE);
                _NOP();
                break;

            // The default is executed for the momentary state
            // ST_ENUM_IN_PROGRESS.  Usually, this state only last a few
            // seconds.  Be sure not to enter LPM3 in this state; USB
            // communication is taking place here, and therefore the mode must
            // be LPM0 or active-CPU.
            case ST_ENUM_IN_PROGRESS:
            default:;
        }

        if (send_ack)
        {
            retVal = cdcSendDataInBackground((uint8_t *) dataBuffer, dataBuffer[1] + 2, CDC0_INTFNUM, 1);
            send_ack = 0;
        }

        //if (ReceiveError || SendError){
        if (retVal)
        {
            // TO DO: User can place code here to handle error
        }
    }  //while(1)
}                               // main()

/*  
 * ======== UNMI_ISR ========
 */
#if defined(__TI_COMPILER_VERSION__) || (__IAR_SYSTEMS_ICC__)
#pragma vector = UNMI_VECTOR
__interrupt void UNMI_ISR (void)
#elif defined(__GNUC__) && (__MSP430__)
void __attribute__ ((interrupt(UNMI_VECTOR))) UNMI_ISR (void)
#else
#error Compiler not found!
#endif
{
    switch (__even_in_range(SYSUNIV, SYSUNIV_BUSIFG ))
    {
        case SYSUNIV_NONE:
            __no_operation();
            break;
        case SYSUNIV_NMIIFG:
            __no_operation();
            break;
        case SYSUNIV_OFIFG:
#ifndef DRIVERLIB_LEGACY_MODE
            UCS_clearFaultFlag(UCS_XT2OFFG);
            UCS_clearFaultFlag(UCS_DCOFFG);
            SFR_clearInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);
#else
            UCS_clearFaultFlag(UCS_BASE, UCS_XT2OFFG);
            UCS_clearFaultFlag(UCS_BASE, UCS_DCOFFG);
            SFR_clearInterrupt(SFR_BASE, SFR_OSCILLATOR_FAULT_INTERRUPT);

#endif
            break;
        case SYSUNIV_ACCVIFG:
            __no_operation();
            break;
        case SYSUNIV_BUSIFG:
            // If the CPU accesses USB memory while the USB module is
            // suspended, a "bus error" can occur.  This generates an NMI.  If
            // USB is automatically disconnecting in your software, set a
            // breakpoint here and see if execution hits it.  See the
            // Programmer's Guide for more information.
            SYSBERRIV = 0; // clear bus error flag
            USB_disable(); // Disable
    }
}

//Released_Version_4_20_00
