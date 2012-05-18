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
#include "rssitest.h"


/*******************************************************************************
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
uint8_t process_rx( uint8_t* buffer, uint8_t size )
{
  //Initialize
  static packet_header_t* header;
  static packet_footer_t* footer; 
  header = (packet_header_t*)buffer;
  // Add one to account for the byte with the packet length
  footer = (packet_footer_t*)(buffer + header->length + 1 );
  static uint8_t* tx_data = ( (packet_data_t*)(tx_buffer + sizeof(packet_header_t)) )->samples;
  static int i;

  if( header->type == 0xAB && header->flags == 0x55 ){
    //Send Recived RSSI values from WBAN out OTA in next TX packet
    pack_recv_rssi_in_tx( header, footer, tx_data, buffer );
    
    //If next packet id is seen, clear all stale RSSIs
    if( buffer[6+2] != packet_id_rx ){
      if( buffer[6+2] < packet_id_rx )
	packet_group++;

      packet_id_rx = buffer[6+2];
      //init_rssi_array();
      buttonPressed++;
      //signal_yellow();
    }
    
    //Print stats for this packet from WBAN
    //uart_write( "#", 1 );
    print_rssi( DEVICE_ADDRESS, &header->source, &footer->rssi, buffer[6+2], packet_group);
  }

  //Process any recv packets
  //check for correct packet
  if( header->type == 0xAA && header->flags == 0x55 ){
    //TODO:Print AP-AP RSSI once a while
    
    //Packet from another AP
    //More generalized code where each pkt can have >1 rssi
    for( i = 6; 		//first group of data starts at buffer[6]
	i < (buffer[5] + 6);	//Buffer[5] = num data bytes in pkt
	i+=3)			//3 bytes per data group
    {
	//If next packet id is seen, clear all stale RSSIs
	if( buffer[i+2] != packet_id_rx ){
	  if( buffer[6+2] < packet_id_rx )
	    packet_group++;
	  
	  packet_id_rx = buffer[i+2];
	  //init_rssi_array();
	  buttonPressed++;
	  //signal_yellow();
	}
	packet_rssi[ header->source ] = buffer[i+1];
      
	print_rssi( header->source, 	//device which sent pkt
		    &buffer[i], 	//orig pkt src as seen by remote device
		    &buffer[i+1],	//rssi of pkt seen by remote device
		    buffer[i+2], 	//packet_id of packet seen by remote
		    packet_group);	//header to distinguish waves of pids
    }
  }
    
  //Pulse Red LED during recieve
  signal_rx();
  return 1;
}


inline void process_tx( uint8_t* tx_data ){
      tx_buffer_cnt = 0;		//ideally semaphore for tx_buffer_cnt
      
      radio_tx( tx_buffer, sizeof(tx_buffer) );
      signal_tx();    // Pulse LED during Transmit
}


void pack_recv_rssi_in_tx( packet_header_t* header, packet_footer_t* footer,
			   uint8_t* tx_data, uint8_t* buffer ){
   //Send Recived RSSI values out OTA in next TX packet

  //Need semaphore for tx_buffer_cnt		
  tx_data[2 + tx_buffer_cnt] = header->source;	//pkt sender with following rssi
  tx_data[3 + tx_buffer_cnt] = footer->rssi;	//Recv RSSI
  tx_data[4 + tx_buffer_cnt] = buffer[6+2];	//Packet ID, copy from recv
  tx_buffer_cnt+=3;				//Increment num bytes used
  tx_data[1] = tx_buffer_cnt;			//Store # bytes used in next pkt
}


/*******************************************************************************
 * @fn     uint8_t fake_button_press()
 * @brief  Instead of using buttons, this function is called from a timer isr
 * ****************************************************************************/
uint8_t timer_callback()
{
  //buttonPressed = 1;
  
  //Disable timer until next packet seen
  //clear_ccr(TOTAL_CCRS);
  //signal_yellow();
  return 1;
}


//Initialize all rssi vals to below noise floor: -138 dBm
inline void init_rssi_array(){
  int j;
  for ( j=0; j < RADIO_NUM_APS; j++ ){
      packet_rssi[j] = 128;
  }
}


inline void signal_rx(){
  //AP - Vibe Dev
  led3_toggle();
}


inline void signal_tx(){
  //AP - Vibe Dev
  led1_toggle();
}


inline void signal_yellow(){
  //AP - Vibe Dev
  led2_toggle();
}

void wait_loop( int count ){
  int i,j;
  for( i=0; i<count ; i++ ){
    for( j=0; j<1000/2; j++){
      ;
    }
  }
}

int main( void )
{
  uint8_t j;
  uint8_t* tx_data;
  packet_header_t* header;

  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  
  header = (packet_header_t*)tx_buffer;
  tx_data = ( (packet_data_t*)(tx_buffer + sizeof(packet_header_t)) )->samples;
  
  // Initialize Tx Buffer
  header->length = PACKET_LEN;
  header->source = DEVICE_ADDRESS;

  header->type = 0xAA;
  header->flags = 0x55;
  
  // Fill in dummy values for the buffer
  for( j=0; j < TOTAL_SAMPLES; j++ )
  {
    //tx_data->samples[j] = j;
    tx_data[j] = 0;
  }
  
  //Initialize all rssi vals to below noise floor, -138 dBm
  init_rssi_array();

  // Make sure processor is running at 12MHz
  setup_oscillator();
  
  // Initialize UART for communications at 115200baud
  setup_uart();
   
  // Initialize LEDs
  setup_leds();
  
  // Initialize timer
  setup_timer_a(MODE_UP);
  
  // Send fake button press every ~2 seconds
  //register_timer_callback( timer_callback, 0 );
  //set_ccr(0, 16400);	// 1/2 second
  
  // Initialize radio and enable receive callback function
  setup_radio( process_rx );
  
  // Enable interrupts, otherwise nothing will work
  eint();
  
  //Tx Header
  tx_data[0] = 0x7A;
  num_tx = 0;
  
  while (1)
  {
    // Enter sleep mode
    //__bis_SR_register( LPM3_bits + GIE );

    if (buttonPressed){ // Process a button press->transmit
      buttonPressed = 0;
      signal_yellow();
      wait_loop( DEVICE_ADDRESS * 1200 );	//Wait device id * 100 ms
      process_tx( tx_data );			//Transmit data
      num_tx++;
    }
  }
  
  return 0;
}
