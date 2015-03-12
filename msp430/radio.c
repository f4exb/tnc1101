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