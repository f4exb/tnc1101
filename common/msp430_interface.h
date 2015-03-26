#ifndef _MSP430_INTERFACE_H_
#define _MSP430_INTERFACE_H_

#include <stdint.h>

#define F_XTAL_MHZ 26
#define MCLK_MHZ 24

typedef enum msp430_block_type_e
{
    MSP430_BLOCK_TYPE_NONE = 0,
    MSP430_BLOCK_TYPE_INIT,
    MSP430_BLOCK_TYPE_TX,
    MSP430_BLOCK_TYPE_TX_KO,
    MSP430_BLOCK_TYPE_RX,
    MSP430_BLOCK_TYPE_RX_KO,
    MSP430_BLOCK_TYPE_RX_CANCEL,
    MSP430_BLOCK_TYPE_RADIO_STATUS,    
    MSP430_BLOCK_TYPE_ECHO_TEST,
    MSP430_BLOCK_TYPE_ERROR
} msp430_block_type_t;

typedef enum sync_word_e
{
    NO_SYNC = 0,              // No preamble/sync
    SYNC_15_OVER_16,          // 15/16 sync word bits detected
    SYNC_16_OVER_16,          // 16/16 sync word bits detected
    SYNC_30_over_32,          // 30/32 sync word bits detected
    SYNC_CARRIER,             // No preamble/sync, carrier-sense above threshold
    SYNC_15_OVER_16_CARRIER,  // 15/16 + carrier-sense above threshold
    SYNC_16_OVER_16_CARRIER,  // 16/16 + carrier-sense above threshold
    SYNC_30_over_32_CARRIER   // 30/32 + carrier-sense above threshold
} sync_word_t;

typedef enum radio_modulation_e {
    RADIO_MOD_NONE = 0,
    RADIO_MOD_OOK,
    RADIO_MOD_FSK2,
    RADIO_MOD_FSK4,
    RADIO_MOD_MSK,
    RADIO_MOD_GFSK,
    RADIO_NUM_MOD
} radio_modulation_t;

typedef enum packet_config_e 
{
    PKTLEN_FIXED = 0,
    PKTLEN_VARIABLE,
    PKTLEN_INFINITE
} packet_config_t;

struct msp430_radio_parms_s
{
	uint8_t  packet_length;  // PACKET_LENGTH      Packet length byte
    uint8_t  packet_config;  // LENGTH_CONFIG[1:0] 2 bit packet length configuration
    uint8_t  preamble_word;  // NUM_PREAMBLE[2:0]  3 bit preamble bytes setting
    uint8_t  sync_word;      // SYNC_MODE[2:0]     3 bit synchronization mode
    uint8_t  drate_e;        // DRATE_E[3:0]       4 bit data rate exponent
    uint8_t  drate_m;        // DRATE_M[7:0]       8 bit data rate mantissa
    uint8_t  deviat_e;       // DEVIATION_E[2:0]   3 bit deviation exponent
    uint8_t  deviat_m;       // DEVIATION_M[2:0]   3 bit deviation mantissa
    uint8_t  chanbw_e;       // CHANBW_E[1:0]      2 bit channel bandwidth exponent
    uint8_t  chanbw_m;       // CHANBW_M[1:0]      2 bit channel bandwidth mantissa
    uint8_t  chanspc_e;      // CHANSPC_E[1:0]     2 bit channel spacing exponent
    uint8_t  chanspc_m;      // CHANSPC_M[7:0]     8 bit channel spacing mantissa
    uint8_t  if_word;        // FREQ_IF[4:0]       5 bit intermediate frequency word
    uint8_t  mod_word;       // MOD_FORMAT[2:0]    3 bit modulation format word
    uint8_t  fec_whitening;  // FEC (bit 0) and Data Whitening (bit 1)
    uint32_t freq_word;      // FREQ[23:0]        24 bit frequency word (FREQ0..FREQ2)
} __attribute__((packed));

typedef struct msp430_radio_parms_s msp430_radio_parms_t;


#endif // _MSP430_INTERFACE_H_