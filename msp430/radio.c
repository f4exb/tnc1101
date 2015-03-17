/******************************************************************************/
/* tnc1101  - Radio serial link using CC1101 module and MSP430                */
/*                                                                            */
/* Radio link interface                                                       */
/*                                                                            */
/*                      (c) Edouard Griffiths, F4EXB, 2015                    */
/*                                                                            */
/******************************************************************************/

#include "radio.h"
#include "TI_CC_CC1100-CC2500.h"

uint8_t bytes_remaining;
uint8_t bytes_sent;

// ------------------------------------------------------------------------------------------------
// Initialize SPI radio interface
void init_radio_spi()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_SPISetup();
    TI_CC_PowerupResetCCxxxx();
}

// ------------------------------------------------------------------------------------------------
// Set up radio
void init_radio(msp430_radio_parms_t *radio_parms)
// ------------------------------------------------------------------------------------------------
{
    uint8_t reg_word;

    // Write register settings

    // IOCFG2 = 0x00: Set in Rx mode (0x02 for Tx mode)
    // o 0x00: Asserts when RX FIFO is filled at or above the RX FIFO threshold. 
    //         De-asserts when RX FIFO is drained below the same threshold.
    // o 0x02: Asserts when the TX FIFO is filled at or above the TX FIFO threshold.
    //         De-asserts when the TX FIFO is below the same threshold.
    TI_CC_SPIWriteReg(TI_CCxxx0_IOCFG2,   0x00); // GDO2 output pin config.

    // IOCFG0 = 0x06: Asserts when sync word has been sent / received, and de-asserts at the
    // end of the packet. In RX, the pin will de-assert when the optional address
    // check fails or the RX FIFO overflows. In TX the pin will de-assert if the TX
    // FIFO underflows:    
    TI_CC_SPIWriteReg(TI_CCxxx0_IOCFG0,   0x06); // GDO0 output pin config.

    // FIFO_THR = 14: 
    // o 5 bytes in TX FIFO (55 available spaces)
    // o 60 bytes in the RX FIFO
    TI_CC_SPIWriteReg(TI_CCxxx0_FIFOTHR,  0x0E); // FIFO threshold.

    // PKTLEN: packet length up to 255 bytes. 
    TI_CC_SPIWriteReg(TI_CCxxx0_PKTLEN, radio_parms->packet_length); // Packet length.

    // PKTCTRL0: Packet automation control #0
    // . bit  7:   unused
    // . bit  6:   x  -> data whitening. Taken from radio config.
    // . bits 5:4: 00 -> normal mode use FIFOs for Rx and Tx
    // . bit  3:   unused
    // . bit  2:   1  -> CRC enabled
    // . bits 1:0: xx -> Packet length mode. Taken from radio config.
    reg_word = ((radio_parms->fec_whitening & 0x02)<<5) + 0x04 + (uint8_t) (radio_parms->packet_config & 0x03);
    TI_CC_SPIWriteReg(TI_CCxxx0_PKTCTRL0, reg_word); // Packet automation control.

    // PKTCTRL1: Packet automation control #1
    // . bits 7:5: 000 -> Preamble quality estimator threshold
    // . bit  4:   unused
    // . bit  3:   0   -> Automatic flush of Rx FIFO disabled (too many side constraints see doc)
    // . bit  2:   1   -> Append two status bytes to the payload (RSSI and LQI + CRC OK)
    // . bits 1:0: 00  -> No address check of received packets
    TI_CC_SPIWriteReg(TI_CCxxx0_PKTCTRL1, 0x04); // Packet automation control.

    TI_CC_SPIWriteReg(TI_CCxxx0_ADDR,     0x00); // Device address for packet filtration (unused, see just above).
    TI_CC_SPIWriteReg(TI_CCxxx0_CHANNR,   0x00); // Channel number (unused, use direct frequency programming).

    // FSCTRL0: Frequency offset added to the base frequency before being used by the
    // frequency synthesizer. (2s-complement). Multiplied by Fxtal/2^14
    TI_CC_SPIWriteReg(TI_CCxxx0_FSCTRL0,  0x00); // Freq synthesizer control.

    // FSCTRL1: The desired IF frequency to employ in RX. Subtracted from FS base frequency
    // in RX and controls the digital complex mixer in the demodulator. Multiplied by Fxtal/2^10
    // Here 0.3046875 MHz (lowest point below 310 kHz)
    TI_CC_SPIWriteReg(TI_CCxxx0_FSCTRL1, (radio_parms->if_word & 0x1F)); // Freq synthesizer control.

    // FREQ2..0: Base frequency for the frequency sythesizer
    // Fo = (Fxosc / 2^16) * FREQ[23..0]
    // FREQ2 is FREQ[23..16]
    // FREQ1 is FREQ[15..8]
    // FREQ0 is FREQ[7..0]
    // Fxtal = 26 MHz and FREQ = 0x10A762 => Fo = 432.99981689453125 MHz
    TI_CC_SPIWriteReg(TI_CCxxx0_FREQ2,    ((radio_parms->freq_word>>16) & 0xFF)); // Freq control word, high byte
    TI_CC_SPIWriteReg(TI_CCxxx0_FREQ1,    ((radio_parms->freq_word>>8)  & 0xFF)); // Freq control word, mid byte.
    TI_CC_SPIWriteReg(TI_CCxxx0_FREQ0,    (radio_parms->freq_word & 0xFF));       // Freq control word, low byte.

    // MDMCFG4 Modem configuration - bandwidth and data rate exponent
    // High nibble: Sets the decimation ratio for the delta-sigma ADC input stream hence the channel bandwidth
    // . bits 7:6: 0  -> CHANBW_E: exponent parameter (see next)
    // . bits 5:4: 2  -> CHANBW_M: mantissa parameter as per:
    //      BW = Fxosc / 8(4+CHANBW_M).2^CHANBW_E => Here: BW = 26/48 MHz = 541.67 kHz
    //      Factory defaults: M=0, E=1 => BW = 26/128 ~ 203 kHz
    // Low nibble:
    // . bits 3:0: 13 -> DRATE_E: data rate base 2 exponent => here 13 (multiply by 8192)
    reg_word = (radio_parms->chanbw_e<<6) + (radio_parms->chanbw_m<<4) + radio_parms->drate_e;
    TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG4,  reg_word); // Modem configuration.

    // MDMCFG3 Modem configuration: DRATE_M data rate mantissa as per formula:
    //    Rate = (256 + DRATE_M).2^DRATE_E.Fxosc / 2^28 
    // Here DRATE_M = 59, DRATE_E = 13 => Rate = 250 kBaud
    TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG3,  radio_parms->drate_m); // Modem configuration.

    // MDMCFG2 Modem configuration: DC block, modulation, Manchester, sync word
    // o bit 7:    0   -> Enable DC blocking (1: disable)
    // o bits 6:4: xxx -> Modulation word. Taken from radio config.
    // o bit 3:    0   -> Manchester disabled (1: enable)
    // o bits 2:0: 011 -> Sync word qualifier (30/32 by default). Taken from radio config.
    reg_word = (radio_parms->mod_word<<4) + radio_parms->sync_word;
    TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG2,  reg_word); // Modem configuration.

    // MDMCFG1 Modem configuration: FEC, Preamble, exponent for channel spacing
    // o bit 7:    x   -> FEC. Taken from radio config.
    // o bits 6:4: 2   -> number of preamble bytes (0:2, 1:3, 2:4, 3:6, 4:8, 5:12, 6:16, 7:24)
    // o bits 3:2: unused
    // o bits 1:0: CHANSPC_E: exponent of channel spacing (here: 2)
    reg_word = ((radio_parms->fec_whitening&0x01)<<7) + (((uint8_t) radio_parms->preamble_word)<<4) + (radio_parms->chanspc_e);
    TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG1,  reg_word); // Modem configuration.

    // MODCFG0 Modem configuration: CHANSPC_M: mantissa of channel spacing following this formula:
    //    Df = (Fxosc / 2^18) * (256 + CHANSPC_M) * 2^CHANSPC_E
    //    Here: (26 /  ) * 2016 = 0.199951171875 MHz (200 kHz)
    TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG0,  radio_parms->chanspc_m); // Modem configuration.

    // DEVIATN: Modem deviation
    // o bit 7:    0   -> not used
    // o bits 6:4: 0   -> DEVIATION_E: deviation exponent
    // o bit 3:    0   -> not used
    // o bits 2:0: 0   -> DEVIATION_M: deviation mantissa
    //
    //   Modulation  Formula
    //
    //   2-FSK    |  
    //   4-FSK    :  Df = (Fxosc / 2^17) * (8 + DEVIATION_M) * 2^DEVIATION_E : Here: 1.5869140625 kHz
    //   GFSK     |
    //
    //   MSK      :  Tx: not well documented, Rx: no effect
    //
    //   OOK      : No effect
    //    
    reg_word = (radio_parms->deviat_e<<4) + (radio_parms->deviat_m);
    TI_CC_SPIWriteReg(TI_CCxxx0_DEVIATN,  reg_word); // Modem dev (when FSK mod en)

    // MCSM2: Main Radio State Machine. See documentation.
    TI_CC_SPIWriteReg(TI_CCxxx0_MCSM2 ,   0x00); //MainRadio Cntrl State Machine

    // MCSM1: Main Radio State Machine. 
    // o bits 7:6: not used
    // o bits 5:4: CCA_MODE: Clear Channel Indicator 
    //   0 (00): Always clear
    //   1 (01): Clear if RSSI below threshold
    //   2 (10): Always claar unless receiving a packet
    //   3 (11): Claar if RSSI below threshold unless receiving a packet
    // o bits 3:2: RXOFF_MODE: Select to what state it should go when a packet has been received
    //   0 (00): IDLE 
    //   1 (01): FSTXON
    //   2 (10): TX
    //   3 (11): RX (stay) <==
    // o bits 1:0: TXOFF_MODE: Select what should happen when a packet has been sent
    //   0 (00): IDLE <==
    //   1 (01): FSTXON
    //   2 (10): TX (stay)
    //   3 (11): RX 
    TI_CC_SPIWriteReg(TI_CCxxx0_MCSM1 ,   0x3C); //MainRadio Cntrl State Machine

    // MCSM0: Main Radio State Machine.
    // o bits 7:6: not used
    // o bits 5:4: FS_AUTOCAL: When to perform automatic calibration
    //   0 (00): Never i.e. manually via strobe command
    //   1 (01): When going from IDLE to RX or TX (or FSTXON)
    //   2 (10): When going from RX or TX back to IDLE automatically
    //   3 (11): Every 4th time when going from RX or TX to IDLE automatically
    // o bits 3:2: PO_TIMEOUT: 
    //   Value : Exp: Timeout after XOSC start
    //   0 (00):   1: Approx. 2.3 – 2.4 μs
    //   1 (01):  16: Approx. 37 – 39 μs
    //   2 (10):  64: Approx. 149 – 155 μs
    //   3 (11): 256: Approx. 597 – 620 μs
    // o bit 1: PIN_CTRL_EN:   Enables the pin radio control option
    // o bit 0: XOSC_FORCE_ON: Force the XOSC to stay on in the SLEEP state.
    TI_CC_SPIWriteReg(TI_CCxxx0_MCSM0 ,   0x18); //MainRadio Cntrl State Machine

    // FOCCFG: Frequency Offset Compensation Configuration.
    // o bits 7:6: not used
    // o bit 5:    If set, the demodulator freezes the frequency offset compensation and clock
    //             recovery feedback loops until the CS signal goes high.
    // o bits 4:3: The frequency compensation loop gain to be used before a sync word is detected.
    //   0 (00): K
    //   1 (01): 2K
    //   2 (10): 3K
    //   3 (11): 4K
    // o bit 2: FOC_POST_K: The frequency compensation loop gain to be used after a sync word is detected.
    //   0: Same as FOC_PRE_K
    //   1: K/2
    // o bits 1:0: FOC_LIMIT: The saturation point for the frequency offset compensation algorithm:
    //   0 (00): ±0 (no frequency offset compensation)
    //   1 (01): ±BW CHAN /8
    //   2 (10): ±BW CHAN /4
    //   3 (11): ±BW CHAN /2
    TI_CC_SPIWriteReg(TI_CCxxx0_FOCCFG,   0x1D); // Freq Offset Compens. Config

    // BSCFG:Bit Synchronization Configuration
    // o bits 7:6: BS_PRE_KI: Clock recovery loop integral gain before sync word
    //   0 (00): Ki
    //   1 (01): 2Ki
    //   2 (10): 3Ki
    //   3 (11): 4Ki
    // o bits 5:4: BS_PRE_KP: Clock recovery loop proportional gain before sync word
    //   0 (00): Kp
    //   1 (01): 2Kp
    //   2 (10): 3Kp
    //   3 (11): 4Kp
    // o bit 3: BS_POST_KI: Clock recovery loop integral gain after sync word
    //   0: Same as BS_PRE_KI
    //   1: Ki/2
    // o bit 2: BS_POST_KP: Clock recovery loop proportional gain after sync word
    //   0: Same as BS_PRE_KP
    //   1: Kp
    // o bits 1:0: BS_LIMIT: Data rate offset saturation (max data rate difference)
    //   0 (00): ±0 (No data rate offset compensation performed)
    //   1 (01): ±3.125 % data rate offset
    //   2 (10): ±6.25 % data rate offset
    //   3 (11): ±12.5 % data rate offset
    TI_CC_SPIWriteReg(TI_CCxxx0_BSCFG,    0x1C); //  Bit synchronization config.

    // AGCCTRL2: AGC Control
    // o bits 7:6: MAX_DVGA_GAIN. Allowable DVGA settings
    //   0 (00): All gain settings can be used
    //   1 (01): The highest gain setting can not be used
    //   2 (10): The 2 highest gain settings can not be used
    //   3 (11): The 3 highest gain settings can not be used
    // o bits 5:3: MAX_LNA_GAIN. Maximum allowable LNA + LNA 2 gain relative to the maximum possible gain.
    //   0 (000): Maximum possible LNA + LNA 2 gain
    //   1 (001): Approx. 2.6 dB below maximum possible gain
    //   2 (010): Approx. 6.1 dB below maximum possible gain
    //   3 (011): Approx. 7.4 dB below maximum possible gain
    //   4 (100): Approx. 9.2 dB below maximum possible gain
    //   5 (101): Approx. 11.5 dB below maximum possible gain
    //   6 (110): Approx. 14.6 dB below maximum possible gain
    //   7 (111): Approx. 17.1 dB below maximum possible gain
    // o bits 2:0: MAGN_TARGET: target value for the averaged amplitude from the digital channel filter (1 LSB = 0 dB).
    //   0 (000): 24 dB
    //   1 (001): 27 dB
    //   2 (010): 30 dB
    //   3 (011): 33 dB
    //   4 (100): 36 dB
    //   5 (101): 38 dB
    //   6 (110): 40 dB
    //   7 (111): 42 dB
    TI_CC_SPIWriteReg(TI_CCxxx0_AGCCTRL2, 0xC7); // AGC control.

    // AGCCTRL1: AGC Control
    // o bit 7: not used
    // o bit 6: AGC_LNA_PRIORITY: Selects between two different strategies for LNA and LNA 2 gain
    //   0: the LNA 2 gain is decreased to minimum before decreasing LNA gain
    //   1: the LNA gain is decreased first.
    // o bits 5:4: CARRIER_SENSE_REL_THR: Sets the relative change threshold for asserting carrier sense
    //   0 (00): Relative carrier sense threshold disabled
    //   1 (01): 6 dB increase in RSSI value
    //   2 (10): 10 dB increase in RSSI value
    //   3 (11): 14 dB increase in RSSI value
    // o bits 3:0: CARRIER_SENSE_ABS_THR: Sets the absolute RSSI threshold for asserting carrier sense. 
    //   The 2-complement signed threshold is programmed in steps of 1 dB and is relative to the MAGN_TARGET setting.
    //   0 is at MAGN_TARGET setting.
    TI_CC_SPIWriteReg(TI_CCxxx0_AGCCTRL1, 0x00); // AGC control.

    // AGCCTRL0: AGC Control
    // o bits 7:6: HYST_LEVEL: Sets the level of hysteresis on the magnitude deviation
    //   0 (00): No hysteresis, small symmetric dead zone, high gain
    //   1 (01): Low hysteresis, small asymmetric dead zone, medium gain
    //   2 (10): Medium hysteresis, medium asymmetric dead zone, medium gain
    //   3 (11): Large hysteresis, large asymmetric dead zone, low gain
    // o bits 5:4: WAIT_TIME: Sets the number of channel filter samples from a gain adjustment has
    //   been made until the AGC algorithm starts accumulating new samples.
    //   0 (00):  8
    //   1 (01): 16
    //   2 (10): 24
    //   3 (11): 32
    // o bits 3:2: AGC_FREEZE: Control when the AGC gain should be frozen.
    //   0 (00): Normal operation. Always adjust gain when required.
    //   1 (01): The gain setting is frozen when a sync word has been found.
    //   2 (10): Manually freeze the analogue gain setting and continue to adjust the digital gain. 
    //   3 (11): Manually freezes both the analogue and the digital gain setting. Used for manually overriding the gain.
    // o bits 0:1: FILTER_LENGTH: 
    //   2-FSK, 4-FSK, MSK: Sets the averaging length for the amplitude from the channel filter.    |  
    //   ASK ,OOK: Sets the OOK/ASK decision boundary for OOK/ASK reception.
    //   Value : #samples: OOK/ASK decixion boundary
    //   0 (00):        8: 4 dB
    //   1 (01):       16: 8 dB
    //   2 (10):       32: 12 dB
    //   3 (11):       64: 16 dB  
    TI_CC_SPIWriteReg(TI_CCxxx0_AGCCTRL0, 0xB2); // AGC control.

    // FREND1: Front End RX Configuration
    // o bits 7:6: LNA_CURRENT: Adjusts front-end LNA PTAT current output
    // o bits 5:4: LNA2MIX_CURRENT: Adjusts front-end PTAT outputs
    // o bits 3:2: LODIV_BUF_CURRENT_RX: Adjusts current in RX LO buffer (LO input to mixer)
    // o bits 1:0: MIX_CURRENT: Adjusts current in mixer
    TI_CC_SPIWriteReg(TI_CCxxx0_FREND1,   0xB6); // Front end RX configuration.

    // FREND0: Front End TX Configuration
    // o bits 7:6: not used
    // o bits 5:4: LODIV_BUF_CURRENT_TX: Adjusts current TX LO buffer (input to PA). The value to use
    //   in this field is given by the SmartRF Studio software
    // o bit 3: not used
    // o bits 1:0: PA_POWER: Selects PA power setting. This value is an index to the PATABLE, 
    //   which can be programmed with up to 8 different PA settings. In OOK/ASK mode, this selects the PATABLE
    //   index to use when transmitting a ‘1’. PATABLE index zero is used in OOK/ASK when transmitting a ‘0’. 
    //   The PATABLE settings from index ‘0’ to the PA_POWER value are used for ASK TX shaping, 
    //   and for power ramp-up/ramp-down at the start/end of transmission in all TX modulation formats.
    TI_CC_SPIWriteReg(TI_CCxxx0_FREND0,   0x10); // Front end RX configuration.

    // FSCAL3: Frequency Synthesizer Calibration
    // o bits 7:6: The value to write in this field before calibration is given by the SmartRF
    //   Studio software.
    // o bits 5:4: CHP_CURR_CAL_EN: Disable charge pump calibration stage when 0.
    // o bits 3:0: FSCAL3: Frequency synthesizer calibration result register.
    TI_CC_SPIWriteReg(TI_CCxxx0_FSCAL3,   0xEA); // Frequency synthesizer cal.

    // FSCAL2: Frequency Synthesizer Calibration
    TI_CC_SPIWriteReg(TI_CCxxx0_FSCAL2,   0x0A); // Frequency synthesizer cal.
    TI_CC_SPIWriteReg(TI_CCxxx0_FSCAL1,   0x00); // Frequency synthesizer cal.
    TI_CC_SPIWriteReg(TI_CCxxx0_FSCAL0,   0x11); // Frequency synthesizer cal.
    TI_CC_SPIWriteReg(TI_CCxxx0_FSTEST,   0x59); // Frequency synthesizer cal.

    // TEST2: Various test settings. The value to write in this field is given by the SmartRF Studio software.
    TI_CC_SPIWriteReg(TI_CCxxx0_TEST2,    0x88); // Various test settings.

    // TEST1: Various test settings. The value to write in this field is given by the SmartRF Studio software.
    TI_CC_SPIWriteReg(TI_CCxxx0_TEST1,    0x31); // Various test settings.

    // TEST0: Various test settings. The value to write in this field is given by the SmartRF Studio software.
    TI_CC_SPIWriteReg(TI_CCxxx0_TEST0,    0x09); // Various test settings.
}

// ------------------------------------------------------------------------------------------------
// Get all status registers
void get_radio_status(uint8_t *status_regs)
// ------------------------------------------------------------------------------------------------
{
    uint8_t reg_index = TI_CCxxx0_PARTNUM;
    int ret = 0;

    memset(status_regs, 0, TI_CCxxx0_NUM_STATUS);

    while (reg_index <= TI_CCxxx0_PARTNUM + TI_CCxxx0_NUM_STATUS)
    {
        status_regs[reg_index - TI_CCxxx0_PARTNUM] = (uint8_t) TI_CC_SPIReadStatus(reg_index);
        reg_index++;
    }
}

// ------------------------------------------------------------------------------------------------
// Setup for sending a block of data up to 255 bytes (packet for CC1101)
// byte 0  : data block size
// byte 1  : block countdown
// byte 2+ : data block
// returns the number of bytes left to be sent
uint8_t send_setup(uint8_t *dataBlock)
// ------------------------------------------------------------------------------------------------
{
    uint8_t initial_tx_count; // Number of bytes to send in first batch

    bytes_remaining = dataBlock[0] + 2; // initial count

    TI_CC_SPIWriteReg(TI_CCxxx0_PKTLEN, bytes_remaining); // Packet length.
    TI_CC_SPIWriteReg(TI_CCxxx0_IOCFG2, 0x02); // GDO2 output pin config TX mode

    bytes_sent = (bytes_remaining > TI_CCxxx0_FIFO_SIZE-1 ? TI_CCxxx0_FIFO_SIZE-1 : bytes_remaining);
    TI_CC_SPIWriteBurstReg(TI_CCxxx0_TXFIFO, dataBlock, bytes_sent);
    bytes_remaining -= bytes_sent;

    return bytes_remaining;
}

// ------------------------------------------------------------------------------------------------
// Send more bytes when Tx FIFO gets depleted
// returns the number of bytes left to be sent
uint8_t send_more(uint8_t *dataBlock)
// ------------------------------------------------------------------------------------------------
{
    uint8_t bytes_to_send;

    if (bytes_remaining)
    {
        bytes_to_send = (bytes_remaining < TX_FIFO_REFILL ? bytes_remaining : TX_FIFO_REFILL);
        TI_CC_SPIWriteBurstReg(TI_CCxxx0_TXFIFO, &dataBlock[bytes_sent], bytes_to_send);
        bytes_remaining -= bytes_to_send;
        bytes_sent += bytes_to_send;
    }

    return bytes_remaining;
}

// ------------------------------------------------------------------------------------------------
// Kick-off Tx
void start_tx()
// ------------------------------------------------------------------------------------------------
{
    TI_CC_SPIStrobe(TI_CCxxx0_STX); 
}