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

#define DEBUG 0
#define DEVICE_ADDRESS 0x01

#define TOTAL_SAMPLES (50)
//TI supplied offsets at Ta = 25 deg C, Vcc = 3V with EM430F6137RF90
//CC430F613x, CC430F612x, CC430F513x MSP430 SoC with RF Core (Rev. F) pg86
#define RADIO_RSSI_OFFSET_868MHZ (74)
#define RADIO_RSSI_OFFSET_433MHZ (74)
#define RADIO_NUM_APS (2)
#define PACKET_ID_MAX (255)

uint8_t tx_buffer[PACKET_LEN+1];
volatile uint8_t buttonPressed = 1;
uint8_t print_buffer[200];
uint8_t tx_buffer_cnt = 0;
uint8_t packet_id_tx = 0;
uint8_t packet_id_rx = 0;
uint8_t packet_rssi[RADIO_NUM_APS];	//Current rx packet's RSSIs

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

uint8_t hex_to_string( uint8_t*, uint8_t*, uint8_t );
uint8_t fake_button_press();
uint8_t process_rx( uint8_t*, uint8_t );

inline uint8_t wban_tx();

//Initialize all rssi vals to below noise floor: -138 dBm
inline void init_rssi_array();

inline void signal_rx();
inline void signal_tx();
inline void signal_yellow();

int main( void );

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
                                    uint8_t buffer_in_size  );

/*******************************************************************************
 * @fn     uint8_t fake_button_press()
 * @brief  Instead of using buttons, this function is called from a timer isr
 * ****************************************************************************/
uint8_t fake_button_press();

uint8_t print_rssi( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id );

inline void print_rssi_debug( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id );

inline void print_rssi_csv( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id );

void pack_recv_rssi_in_tx( packet_header_t* header, packet_footer_t* footer,
			   uint8_t* tx_data, uint8_t* buffer );

inline uint8_t process_rx_wban(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data);

void process_rx_debug(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data);

inline uint8_t process_rx_ap(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data);


/*******************************************************************************
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
uint8_t process_rx( uint8_t* buffer, uint8_t size );


