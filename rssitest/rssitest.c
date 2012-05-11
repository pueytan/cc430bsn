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
#include "common.h"
#include <signal.h>
#include <string.h>
#include "leds.h"
#include "oscillator.h"
#include "uart.h"
#include "timers.h"
#include "radio.h"

uint8_t tx_buffer[PACKET_LEN+1];
uint8_t buttonPressed = 1;
uint8_t print_buffer[200];
uint8_t tx_buffer_cnt = 0;
uint8_t packet_id = 1;

#define TOTAL_SAMPLES (50)
//TI supplied offsets at Ta = 25 deg C, Vcc = 3V with EM430F6137RF90
//CC430F613x, CC430F612x, CC430F513x MSP430 SoC with RF Core (Rev. F) pg86
#define RADIO_RSSI_OFFSET_868MHZ (74)
#define RADIO_RSSI_OFFSET_433MHZ (74)


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



inline uint8_t wban_tx(){
  
  return 0;
}

int main( void )
{
  uint8_t buffer_index;
  packet_data_t* tx_data;
  packet_header_t* header;

  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  
  header = (packet_header_t*)tx_buffer;
  tx_data = (packet_data_t*)(tx_buffer + sizeof(packet_header_t));
  
  // Initialize Tx Buffer
  header->length = PACKET_LEN;
  header->source = DEVICE_ADDRESS;
  header->type = 0xAA;
  header->flags = 0x55;
  
  // Fill in dummy values for the buffer
  for( buffer_index=0; buffer_index < TOTAL_SAMPLES; buffer_index++ )
  {
    //tx_data->samples[buffer_index] = buffer_index;
    tx_data->samples[buffer_index] = 0;
  }  
  
  // Make sure processor is running at 12MHz
  setup_oscillator();
  
  // Initialize UART for communications at 115200baud
  setup_uart();
   
  // Initialize LEDs
  setup_leds();
  
  // Initialize timer
  setup_timer_a(MODE_CONTINUOUS);
  
  // Send fake button press every ~2 seconds
  register_timer_callback( fake_button_press, TOTAL_CCRS );

  // Initialize radio and enable receive callback function
  setup_radio( process_rx );
  
  // Enable interrupts, otherwise nothing will work
  eint();
   
  while (1)
  {
    // Enter sleep mode
    __bis_SR_register( LPM3_bits + GIE );
    __no_operation();
    __no_operation();
    if (buttonPressed) // Process a button press->transmit
    {
      led1_toggle(); // Pulse LED during Transmit
      buttonPressed = 0;
      
      if( DEVICE_ADDRESS > 9 ){
	wban_tx();
      }
      
      radio_tx( tx_buffer, sizeof(tx_buffer) );
      //Need semaphore for tx_buffer_cnt;
      tx_buffer_cnt = 0;
      //uart_write( "\r\nTx\r\n", 6 );
    }    
  }
  
  return 0;
}

/*******************************************************************************
 * @fn     uint8_t hex_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  Used to convert hex values to [hex]string format
 * ****************************************************************************/
uint8_t hex_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size  )
{
  static const uint8_t hex_char[16] = { '0', '1', '2', '3', '4', '5', '6', '7', 
                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  uint8_t counter = 0;
  
  while(counter < buffer_in_size*2 )
  {
    buffer_out[counter] = hex_char[((buffer_in[(counter>>1)]>>4) & 0xF)];
    counter++;
    buffer_out[counter] = hex_char[(buffer_in[(counter>>1)] & 0xF)];
    counter++;
  }
  
  return counter;
}

/*******************************************************************************
 * @fn     uint8_t rssi_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  Used to convert raw rssi values to decimal string format
 * @return Size of string in buffer_out, excluding \0
 * ****************************************************************************/
uint8_t rssi_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size  )
{
  uint8_t counter = 0;
  
  while(counter < buffer_in_size*2 )
  {
    int rssi_dbm;
    int rssi_dec = buffer_in[(counter>>1)] & 0xFF;
    if( rssi_dec >= 128 ){
	rssi_dec = ( rssi_dec - 256 );
    }
    rssi_dbm = rssi_dec / 2 - RADIO_RSSI_OFFSET_868MHZ;
    
    counter += sprintf( &buffer_out[counter], "%d", rssi_dbm );
  }
  return counter;
}


/*******************************************************************************
 * @fn     uint8_t int_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  Used to convert hex values to [decimal]string format
 * @return Size of string in buffer_out, excluding \0
 * ****************************************************************************/
uint8_t int_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size  )
{
  uint8_t cnt = 0;
  
  while(cnt < buffer_in_size*2 )
  {  
    cnt += sprintf( &buffer_out[cnt], "%d", buffer_in[cnt>>1] & 0xFF );
  }
  return cnt;
}

/*******************************************************************************
 * @fn     uint8_t fake_button_press()
 * @brief  Instead of using buttons, this function is called from a timer isr
 * ****************************************************************************/
uint8_t fake_button_press()
{
  buttonPressed = 1;
  
  return 1;
}

uint8_t print_rssi( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id ){
  uint8_t bufLen = 0;
  
  //Disable printing if high numbered device (no uart available)
  if ( DEVICE_ADDRESS > 3 ){
    return 0;
  }
  
  uart_write( "[", 1 );
  hex_to_string( print_buffer, &pkt_reciever, 1 );
  uart_write( print_buffer, 2 );
  uart_write( "] ", 2 );
   
  uart_write( "Pkt From: ", 10 );
  hex_to_string( print_buffer, pkt_source, 1 );
  uart_write( print_buffer, 2 );
  uart_write( " ", 1 );
  
  uart_write( "PID: ", 5 );
  hex_to_string( print_buffer, &pkt_id, 1 );
  uart_write( print_buffer, 2 );
  uart_write( " ", 1 );
  
  uart_write( "RSSI: 0x", 8 );
  hex_to_string( print_buffer, pkt_rssi, 1 );
  uart_write( print_buffer, 2 );
  uart_write( " ", 1 );
 
  uart_write( "RSSI: ", 6 );
  bufLen = rssi_to_string( print_buffer, pkt_rssi, 1 );
  uart_write( print_buffer, bufLen );
  uart_write( " dBm \n", 6 );
  
  //led2_toggle();
  
  return 1;
}


void pack_recv_rssi_in_tx( packet_header_t* header, packet_footer_t* footer,
			   uint8_t* tx_data, uint8_t* buffer ){
   //Send Recived RSSI values out OTA in next TX packet

  tx_data[0] = 0x7A;				
  //Need semaphore for tx_buffer_cnt		//Header
  tx_data[2 + tx_buffer_cnt] = header->source;	//pkt sender with following rssi
  tx_data[3 + tx_buffer_cnt] = footer->rssi;	//Recv RSSI
#if DEVICE_ADDRESS < 0x0A  
  tx_data[4 + tx_buffer_cnt] = buffer[6+2];	//Packet ID, sequential
#else
  tx_data[4 + tx_buffer_cnt] = packet_id++;	//Packet ID, sequential
#endif
  tx_buffer_cnt+=3;				//Increment num bytes used
  tx_data[1] = tx_buffer_cnt;			//Store # bytes used in next pkt
   
}

inline uint8_t process_rx_wban(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data){
  
  //Send Recived RSSI values out OTA in next TX packet
  pack_recv_rssi_in_tx( header, footer, tx_data, buffer );
  
  return 0;
}

void process_rx_debug(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data){
  // Print incoming packet information for debugging
//   uart_write( "Size: ", 6 );
//   hex_to_string( print_buffer, &header->length, 1 );
//   uart_write( print_buffer, 2 );
//   //uart_write( "\r\n", 2 );

//   uart_write( " Type: ", 7 );
//   hex_to_string( print_buffer, &header->type, 1 );
//   uart_write( print_buffer, 2 );
//   uart_write( "\r\n", 2 );
//   
//   uart_write( " Flags: ", 8 );
//   hex_to_string( print_buffer, &header->flags, 1 );
//   uart_write( print_buffer, 2 );
//   uart_write( "\r\n", 2 ); 

    //Print whole packet in hex
//   hex_to_string( print_buffer, buffer, size );
//   uart_write( print_buffer, (size)*2 );
//   uart_write( "\r\n", 2 );
}

inline uint8_t process_rx_ap(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer, uint8_t* tx_data){
  int i;
  
  //Print stats for this recv packet
  uart_write( "!", 1 );
  print_rssi( DEVICE_ADDRESS, &header->source, &footer->rssi, buffer[6+2] );
  
  //Send Recived RSSI values out OTA in next TX packet
  pack_recv_rssi_in_tx( header, footer, tx_data, buffer );
  
  //Process any recv packets
  //check for correct packet
  if( header->type == 0xAA && header->flags == 0x55 ){
    //More generalized code where each pkt can have >1 rssi
    for( i = 6; 		//first group of data starts at buffer[6]
	i < (buffer[5] + 6);	//Buffer[5] = num data bytes in pkt
	i+=3)			//3 bytes per data group
    {
      //if ( header->source != DEVICE_ADDRESS ){
	uart_write( "#", 1 );
	print_rssi( header->source, 	//device which sent pkt
		    &buffer[i], 	//orig pkt src as seen by remote device
		    &buffer[i+1],	//rssi of pkt seen by remote device
		    buffer[i+2] );	//packet_id of packet seen by remote
      //}
    }
  }

  return 0;
}



/*******************************************************************************
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
uint8_t process_rx( uint8_t* buffer, uint8_t size )
{
  //Initialize
  packet_header_t* header;
  header = (packet_header_t*)buffer;
  packet_footer_t* footer; 
  uint8_t* tx_data = ( (packet_data_t*)(tx_buffer + sizeof(packet_header_t)) )->samples;
  
  // Add one to account for the byte with the packet length
  footer = (packet_footer_t*)(buffer + header->length + 1 );
  
#if DEVICE_ADDRESS < 0x0A
  process_rx_ap( buffer, size, header, footer, tx_data );
#else
  process_rx_wban( buffer, size, header, footer, tx_data );
#endif
  
  // Erase buffer just for fun
  memset( buffer, 0x00, size );
  
  //Pulse LED3 during recieve
  led3_toggle();
  return 1;
}


