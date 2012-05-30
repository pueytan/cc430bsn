/** @file rssitest.c
*
* @brief  Simple test of radio functions.
*         A timer interrupt sends a message every ~2 seconds and prints out 
*         received RSSI.
*
* @author Puey Wei Tan
* @author Alvaro Prieto
*       derived from work by M. Morales/D. Dang of Texas Instruments
*/
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "intrinsics.h"
#include "leds.h"
#include "timers.h"
#include <signal.h>
#include "oscillator.h"
#include "uart.h"
#include "radio.h"
#include "radio_ext.h"

#define DEBUG 0
#define DEVICE_ADDRESS 0x01

#define RADIO_NUM_APS (4)
#define PACKET_ID_MAX (255)

uint8_t tx_buffer[PACKET_LEN+1];
volatile uint8_t timerCalls = 1;
uint8_t tx_buffer_cnt = 0;
uint8_t packet_id_tx = 0;
uint8_t packet_id_rx = 0;
uint8_t packet_group = 0;
uint8_t packet_rssi[RADIO_NUM_APS];	//Current rx packet's RSSIs
uint8_t hex_to_string( uint8_t*, uint8_t*, uint8_t );
uint8_t process_rx( uint8_t*, uint8_t );
int num_tx;

inline uint8_t wban_tx();

//Initialize all rssi vals to below noise floor: -138 dBm
inline void init_rssi_array();

inline void signal_rx();
inline void signal_tx();
inline void signal_yellow();

int main( void );

void pack_recv_rssi_in_tx( packet_header_t* header, packet_footer_t* footer,
			   uint8_t* tx_data, uint8_t* buffer );

inline uint8_t process_rx_wban(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data);

inline uint8_t process_rx_ap(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data);

/*******************************************************************************
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
uint8_t process_rx( uint8_t* buffer, uint8_t size );
