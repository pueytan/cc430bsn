#ifndef _RADIO_EXT_H
#define _RADIO_EXT_H

#include "uart.h"
#include <stdio.h>

//TI supplied offsets at Ta = 25 deg C, Vcc = 3V with EM430F6137RF90
//CC430F613x, CC430F612x, CC430F513x MSP430 SoC with RF Core (Rev. F) pg86
#define RADIO_RSSI_OFFSET_868MHZ (74)
#define RADIO_RSSI_OFFSET_433MHZ (74)
#define TOTAL_SAMPLES (50)
uint8_t print_buffer[200];
typedef struct
{
  uint8_t length;
  uint8_t source;
  uint8_t type;
  uint8_t flags;
} packet_header_t;

typedef struct
{
  uint8_t samples[TOTAL_SAMPLES];
} packet_data_t;

typedef struct
{
  uint8_t rssi;
  uint8_t lqi_crcok;
} packet_footer_t;

/*******************************************************************************
 * @fn     uint8_t hex_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  Used to convert hex values to [hex]string format
 * ****************************************************************************/
uint8_t hex_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size  );

/*******************************************************************************
 * @fn     uint8_t rssi_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  Used to convert raw rssi values to decimal string format
 * @return Size of string in buffer_out, excluding \0
 * ****************************************************************************/
uint8_t rssi_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size  );


/*******************************************************************************
 * @fn     uint8_t int_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  Used to convert hex values to [decimal]string format
 * @return Size of string in buffer_out, excluding \0
 * ****************************************************************************/
uint8_t int_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size );


uint8_t print_rssi( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id, uint8_t pkt_grp );


inline void print_rssi_debug( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id, uint8_t pkt_grp );


inline void print_rssi_csv( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id, uint8_t pkt_grp );

/*******************************************************************************
 * @fn     void print_rx_debug
 * @brief  Prints detailed incoming packet info. Size, headers, entire packet
 * @return Size of string in buffer_out, excluding \0
 * ****************************************************************************/
void print_rx_debug(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data);

#endif /* _RADIO_EXT_H */
