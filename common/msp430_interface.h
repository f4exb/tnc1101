#ifndef _MSP430_INTERFACE_H_
#define _MSP430_INTERFACE_H_

typedef enum msp430_block_type_e
{
    MSP430_BLOCK_TYPE_INIT = 0,
    MSP430_BLOCK_TYPE_DATA,
    MSP430_BLOCK_TYPE_DATA_ACK,
    MSP430_BLOCK_TYPE_COMMAND,
    MSP430_BLOCK_TYPE_COMMAND_ACK,
    MSP430_BLOCK_TYPE_RADIO_STATUS,    
    MSP430_BLOCK_TYPE_ECHO_TEST,
    MSP430_BLOCK_TYPE_ERROR
} msp430_block_type_t;

struct msp430_radio_parms_s
{
	uint8_t packet_length;
	uint8_t pktctrl0;
} __attribute__((packed));

typedef struct msp430_radio_parms_s msp430_radio_parms_t;


#endif // _MSP430_INTERFACE_H_