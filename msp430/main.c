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

#define BUFFER_SIZE 262                // Command + USB size + size + data (size + block countdown + data + RSSI + LQI)
                                       //       1 +        1 +    1         ------------------------- 256 +    1 +   1  + 1 
uint8_t dataBuffer[BUFFER_SIZE];       // Current I/O buffer
char    outString[65];                 // Holds outgoing strings to be sent
static  uint8_t send_ack = 0;          // Set when an ack is to be sent
static  uint8_t rtx_toggle = 0;        // 0: Rx - 1: Tx
static  uint8_t dataIndex = 0;         // Current index in I/O buffer
static  uint8_t *returnedDataBuffer;   // pointer to data buffer returned via USB

uint8_t gdo0_r, gdo0_f, gdo2_r, gdo2_f;

// = Static functions declarations =================================================================

static void    init_leds();
static void    init_left_button(); 
static void    init_gdo0();
static void    init_gdo0_int();
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
    TI_CC_RED_LED_PxDIR   |=  TI_CC_RED_LED;
    TI_CC_GREEN_LED_PxDIR |=  TI_CC_GREEN_LED;
    TI_CC_RED_LED_PxOUT   &= ~TI_CC_RED_LED;
    TI_CC_GREEN_LED_PxOUT &= ~TI_CC_GREEN_LED;
}

// ------------------------------------------------------------------------------------------------
// Init the board left button with interrupt handler
void init_left_button()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_SWL_PxREN |=  TI_CC_SWL; // Enable pull-up and pull-down
    TI_CC_SWL_PxOUT |=  TI_CC_SWL; // Pull-up
    TI_CC_SWL_PxIE  |=  TI_CC_SWL; // Interrupt enabled
    TI_CC_SWL_PxIES |=  TI_CC_SWL; // Hi/lo falling edge
    TI_CC_SWL_PxIFG &= ~TI_CC_SWL; // IFG cleared just in case
}

// ------------------------------------------------------------------------------------------------
// Init GDOx lines
void init_gdo()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_GDO0_PxSEL &= ~TI_CC_GDO0_PIN; // Set GDO0 as GPIO
    TI_CC_GDO2_PxSEL &= ~TI_CC_GDO2_PIN; // Set GDO2 as GPIO
    TI_CC_GDO0_PxDIR &= ~TI_CC_GDO0_PIN; // Set GDO0 as input
    TI_CC_GDO2_PxDIR &= ~TI_CC_GDO2_PIN; // Set GDO2 as input
    TI_CC_GDO0_PxREN |=  TI_CC_GDO0_PIN; // Enable pull-up and pull-down
    TI_CC_GDO0_PxOUT &= ~TI_CC_GDO0_PIN; // Pull-down
    TI_CC_GDO2_PxREN |=  TI_CC_GDO2_PIN; // Enable pull-up and pull-down
    TI_CC_GDO2_PxOUT &= ~TI_CC_GDO2_PIN; // Pull-down
    TI_CC_GDO0_PxIE  &= ~TI_CC_GDO0_PIN; // Interrupt disabled
    TI_CC_GDO2_PxIE  &= ~TI_CC_GDO2_PIN; // Interrupt disabled
}

// ------------------------------------------------------------------------------------------------
// Init the CC1101 GDO0 (packet) interrupt
void init_gdo0_int()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN; // IFG cleared just in case
    TI_CC_GDO0_PxIE  |=  TI_CC_GDO0_PIN; // Interrupt enabled
    TI_CC_GDO0_PxIES &= ~TI_CC_GDO0_PIN; // Start with rising edge
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
uint8_t process_usb_block(uint16_t count, uint8_t *pDataBuffer)
// ------------------------------------------------------------------------------------------------
{
    uint8_t byte_count = pDataBuffer[1];
    char str_byte[4];

    __disable_interrupt();

    returnedDataBuffer = pDataBuffer;

    if (pDataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_ECHO_TEST)
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
    else if (pDataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_RADIO_STATUS)
    {
        get_radio_status(&pDataBuffer[2]);
        pDataBuffer[1] = TI_CCxxx0_NUM_STATUS;
        send_ack = 1;
    }
    else if (pDataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_INIT)
    {
        set_green_led(0);
        set_red_led(0);
        reset_radio();
        DELAY_US(5000);  // ~5ms delay 
        init_radio((msp430_radio_parms_t *) &pDataBuffer[2]);
        send_ack = 1;
    }
    else if (pDataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_TX)
    {
        rtx_toggle = 1;
        
        if (transmit_setup(&pDataBuffer[1])) // if bytes are left to be sent activate threshold interrupt 
        {
            TI_CC_GDO2_PxIFG &= ~TI_CC_GDO2_PIN; // IFG cleared just in case
            TI_CC_GDO2_PxIE  |=  TI_CC_GDO2_PIN; // Interrupt enabled
            TI_CC_GDO2_PxIES |=  TI_CC_GDO2_PIN; // Threshold on falling edge (hi->lo) - Tx FIFO depletion
        }
        
        init_gdo0_int();
        set_red_led(0);

        start_tx();
    }
    else if (pDataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_RX)
    {
        rtx_toggle = 0;
        set_green_led(0);
        receive_setup(&pDataBuffer[2]);
        init_gdo0_int();
        TI_CC_GDO2_PxIFG &= ~TI_CC_GDO2_PIN; // IFG cleared just in case
        TI_CC_GDO2_PxIE  |=  TI_CC_GDO2_PIN; // Interrupt enabled
        TI_CC_GDO2_PxIES &= ~TI_CC_GDO2_PIN; // Threshold on rising edge (lo->hi) - Rx FIFO filling

        start_rx();
    }
    else if (pDataBuffer[0] == (uint8_t) MSP430_BLOCK_TYPE_RX_CANCEL)
    {
        TI_CC_GDO0_PxIE  &= ~TI_CC_GDO0_PIN; // Interrupt disabled
        TI_CC_GDO2_PxIE  &= ~TI_CC_GDO2_PIN; // Interrupt disabled
        TI_CC_GDO2_PxIFG &= ~TI_CC_GDO2_PIN; // IFG cleared just in case
        TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN; // IFG cleared just in case

        receive_cancel();

        pDataBuffer[1] = 0; // Just send back the command as an ACK
        send_ack = 1;
    }

    __enable_interrupt();  // Enable interrupts globally

    return 0;
}

// = Interrupt handlers ===========================================================================

// ------------------------------------------------------------------------------------------------
// Port 1 interrupt service routine
// GDO2        on P1.4
// GDO0        on P1.5
#if defined(__TI_COMPILER_VERSION__) || (__IAR_SYSTEMS_ICC__)
#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR (void)
#elif defined(__GNUC__) && (__MSP430__)
void __attribute__ ((interrupt(PORT1_VECTOR))) PORT1_ISR (void)
#else
#error Compiler not found!
#endif
// ------------------------------------------------------------------------------------------------
{
    __disable_interrupt();

    switch (__even_in_range(P1IV,16))
    {
        case 0:  // No interrupt
            break;
        case 2:  // P1.0
            break;
        case 4:  // P1.1 
            break;
        case 6:  // P1.2
            break;
        case 8:  // P1.3
            break;
        case 10: // P1.4 : FIFO threshold interrupt
            if (rtx_toggle) // Tx-ing
            {
                gdo2_f++;
                
                if (!transmit_more()) // if no more bytes are left to be sent de-activate threshold interrupt
                {
                    TI_CC_GDO2_PxIE &= ~TI_CC_GDO2_PIN;   // Interrupt disabled
                }
            }
            else // Rx-ing
            {
                gdo2_r++;
                receive_more();
            }

            TI_CC_GDO2_PxIFG &= ~TI_CC_GDO2_PIN; // Clear IFG
            break;
        case 12:  // P1.5 : Packet interrupt
            if (rtx_toggle) // Tx-ing
            {
                toggle_red_led();

                if ((TI_CC_GDO0_PxIES & TI_CC_GDO0_PIN) == 0) // rising edge 
                {
                    gdo0_r++;
                    TI_CC_GDO0_PxIES |= TI_CC_GDO0_PIN;  // Enable falling edge (hi->lo)
                }
                else // falling edge = end of packet
                {
                    uint8_t status;

                    gdo0_f++;
                    status = transmit_end();

                    if (status == 0) 
                    {
                        dataBuffer[0]  = (uint8_t) MSP430_BLOCK_TYPE_TX;   
                    }
                    else // TX FIFO UNDERFLOW or not empty => problem 
                    {
                        dataBuffer[0]  = (uint8_t) MSP430_BLOCK_TYPE_TX_KO;
                        flush_tx_fifo();
                    }

                    dataBuffer[1]  = 9;
                    dataBuffer[2]  = status;
                    dataBuffer[3]  = gdo0_r;
                    dataBuffer[4]  = gdo0_f;
                    dataBuffer[5]  = gdo2_r;
                    dataBuffer[6]  = gdo2_f;
                    dataBuffer[7]  = TI_CC_GDO0_PxIN;
                    dataBuffer[8]  = TI_CC_GDO0_PxIFG;
                    dataBuffer[9]  = TI_CC_GDO0_PxIE;
                    dataBuffer[10] = TI_CC_GDO0_PxIES;
                    returnedDataBuffer = dataBuffer;
                    send_ack = 1;
                    TI_CC_GDO0_PxIE &= ~TI_CC_GDO0_PIN;   // Interrupt disabled
                }
            }
            else // Rx-ing
            {
                toggle_green_led();

                if ((TI_CC_GDO0_PxIES & TI_CC_GDO0_PIN) == 0) // rising edge 
                //if ((TI_CC_GDO0_PxIN & TI_CC_GDO0_PIN) != 0) // rising edge = start of packet
                {
                    gdo0_r++;
                    TI_CC_GDO0_PxIES |= TI_CC_GDO0_PIN;  // Enable falling edge (hi->lo)
                }
                else // falling edge = end of packet
                {
                    uint8_t status;

                    gdo0_f++;
                    status = receive_end();

                    if (status == 0) 
                    {
                        // dataBuffer[1] has 0x01 (size of USB block to start Rx)
                        // so bump returned USB header by 1 byte
                        dataBuffer[1] = (uint8_t) MSP430_BLOCK_TYPE_RX;
                        dataBuffer[2] += 2; // + RSSI + LQI
                        returnedDataBuffer = &dataBuffer[1];
                        // frequency compensation
                        //freq_compensate();
                    }
                    else // RX FIFO OVERFLOW or not empty => problem
                    {
                        dataBuffer[0]  = (uint8_t) MSP430_BLOCK_TYPE_RX_KO;
                        dataBuffer[1]  = 9;
                        dataBuffer[2]  = status;
                        dataBuffer[3]  = gdo0_r;
                        dataBuffer[4]  = gdo0_f;
                        dataBuffer[5]  = gdo2_r;
                        dataBuffer[6]  = gdo2_f;
                        dataBuffer[7]  = TI_CC_GDO0_PxIN;
                        dataBuffer[8]  = TI_CC_GDO0_PxIFG;
                        dataBuffer[9]  = TI_CC_GDO0_PxIE;
                        dataBuffer[10] = TI_CC_GDO0_PxIES;
                        flush_rx_fifo();
                        returnedDataBuffer = dataBuffer;
                    }

                    send_ack = 1;
                    TI_CC_GDO0_PxIE &= ~TI_CC_GDO0_PIN;   // Interrupt disabled
                    TI_CC_GDO2_PxIE &= ~TI_CC_GDO2_PIN;   // Interrupt disabled
                }
            }

            TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN; // Clear IFG
            break;
        case 14:  // P1.6
            break;
        case 16:  // P1.7
            break;
    }

    __enable_interrupt();  // Enable interrupts globally
}

// ------------------------------------------------------------------------------------------------
// Port 2 interrupt service routine
// Left button on P2.1
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

    switch (__even_in_range(P2IV,16))
    {
        case 0:  // No interrupt
            break;
        case 2:  // P2.0
            break;
        case 4:  // P2.1 : Left button
            toggle_red_led(0);
            set_green_led(0);
            TI_CC_SWL_PxIFG &= ~TI_CC_SWL; // Clear IFG
            break;
        case 6:  // P2.2
            break;
        case 8:  // P2.3
            break;
        case 10: // P2.4
            break;
        case 12: // P2.5 
            break;
        case 14: // P2.6
            break;
        case 16: // P2.7
            break;
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
    initClocks(MCLK_MHZ * 1000000);   // Config clocks. MCLK=SMCLK=FLL=8MHz; ACLK=REFO=32kHz
    USB_setup(TRUE, TRUE); // Init USB & events; if a host is present, connect

    DELAY_US(5000);        // 5ms delay to compensate for time to startup between MSP430 and CC1100/2500 
    init_radio_spi();      // Initialize SPI comm with radio module
    init_leds();
    init_freq_offset();    // initialize frequency offset compensation

    P1IE  = 0;
    P1IFG = 0;
    P2IE  = 0;
    P2IFG = 0;

    gdo0_r = 0;
    gdo0_f = 0;
    gdo2_r = 0;
    gdo2_f = 0;

    init_left_button();
    init_gdo();
    memset(dataBuffer, 0, BUFFER_SIZE);

    //__bis_SR_register(LPM0_bits + GIE); // Enter LPM0 until awakened by an event handler

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

                    count = cdcReceiveDataInBuffer((uint8_t*) &dataBuffer[dataIndex], BUFFER_SIZE, CDC0_INTFNUM);

                    if (count >= 2)
                    {
                        if (dataIndex + count < dataBuffer[1] + 2)
                        {
                            dataIndex += count;
                        }
                        else
                        {
                            retVal = process_usb_block(count, (uint8_t*) dataBuffer);
                            dataIndex = 0;

                            if (retVal)
                            {
                                break;
                            }
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

                if (send_ack)
                {
                    if (returnedDataBuffer[0] == 0) // it's a bug!
                    {
                        returnedDataBuffer[1]  = 9;
                        returnedDataBuffer[2]  = rtx_toggle;
                        returnedDataBuffer[3]  = gdo0_r;
                        returnedDataBuffer[4]  = gdo0_f;
                        returnedDataBuffer[5]  = gdo2_r;
                        returnedDataBuffer[6]  = gdo2_f;
                        returnedDataBuffer[7]  = TI_CC_GDO0_PxIN;
                        returnedDataBuffer[8]  = TI_CC_GDO0_PxIFG;
                        returnedDataBuffer[9]  = TI_CC_GDO0_PxIE;
                        returnedDataBuffer[10] = TI_CC_GDO0_PxIES;
                    }

                    retVal = cdcSendDataInBackground((uint8_t *) returnedDataBuffer, returnedDataBuffer[1] + 2, CDC0_INTFNUM, 1);
                    send_ack = 0;
                }

                //__bis_SR_register(LPM0_bits + GIE); // Enter LPM0 until awakened by an event handler
                break; // ST_ENUM_ACTIVE
                
            // These cases are executed while your device is disconnected from
            // the host (meaning, not enumerated); enumerated but suspended
            // by the host, or connected to a powered hub without a USB host
            // present.
            case ST_PHYS_DISCONNECTED:
            case ST_ENUM_SUSPENDED:
            case ST_PHYS_CONNECTED_NOENUM_SUSP:
                //__bis_SR_register(LPM3_bits + GIE);
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
