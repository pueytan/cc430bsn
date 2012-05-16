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
  
  //Pulse Red LED during recieve
  signal_rx();
  return 1;
}


inline void process_tx(){
      signal_tx();    // Pulse LED during Transmit     
      radio_tx( tx_buffer, sizeof(tx_buffer) );
      //Need semaphore for tx_buffer_cnt;
      tx_buffer_cnt = 0;
      //uart_write( "\r\nTx\r\n", 6 );  
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
 
  
#if debug < 99
  //Only print stats for packets from WBAN in csv mode
  if( header->type == 0xAB && header->flags == 0x55 ){
#else  
  //print everything in debug mode to see AP-AP rssi
  if( 1 ){
#endif
    //Print stats for this recv packet
    print_rssi( DEVICE_ADDRESS, &header->source, &footer->rssi, buffer[6+2] );
    //Send Recived RSSI values from WBAN out OTA in next TX packet
    pack_recv_rssi_in_tx( header, footer, tx_data, buffer );
  }

  //Process any recv packets
  //check for correct packet
  if( header->type == 0xAA && header->flags == 0x55 ){
    //More generalized code where each pkt can have >1 rssi
    for( i = 6; 		//first group of data starts at buffer[6]
	i < (buffer[5] + 6);	//Buffer[5] = num data bytes in pkt
	i+=3)			//3 bytes per data group
    {
      //if ( header->source != DEVICE_ADDRESS ){
	print_rssi( header->source, 	//device which sent pkt
		    &buffer[i], 	//orig pkt src as seen by remote device
		    &buffer[i+1],	//rssi of pkt seen by remote device
		    buffer[i+2] );	//packet_id of packet seen by remote
	
	//If next packet is seen, clear all stale RSSIs
	if( buffer[i+2] != packet_id_rx ){
	  packet_id_rx = buffer[i+2];
	  init_rssi_array();
	  signal_yellow();
	}
	  packet_rssi[ header->source ] = buffer[i+1];
      //}
    }
  }

  return 0;
}


void pack_recv_rssi_in_tx( packet_header_t* header, packet_footer_t* footer,
			   uint8_t* tx_data, uint8_t* buffer ){
   //Send Recived RSSI values out OTA in next TX packet

  tx_data[0] = 0x7A;				
  //Need semaphore for tx_buffer_cnt		//Header
  tx_data[2 + tx_buffer_cnt] = header->source;	//pkt sender with following rssi
  tx_data[3 + tx_buffer_cnt] = footer->rssi;	//Recv RSSI
#if DEVICE_ADDRESS < 0x0A  
  tx_data[4 + tx_buffer_cnt] = buffer[6+2];	//Packet ID, copy from recv
#else
  tx_data[4 + tx_buffer_cnt] = packet_id_tx++;	//Packet ID, sequential
#endif
  tx_buffer_cnt+=3;				//Increment num bytes used
  tx_data[1] = tx_buffer_cnt;			//Store # bytes used in next pkt
   
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
    int rssi_raw = buffer_in[(counter>>1)] & 0xFF;
    if( rssi_raw >= 128 ){
	rssi_raw = ( rssi_raw - 256 );
    }
    rssi_dbm = rssi_raw / 2 - RADIO_RSSI_OFFSET_868MHZ;
    
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


uint8_t print_rssi( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id ){
  //Disable printing if high numbered device (no uart available)
  if ( DEVICE_ADDRESS > 9 ){
    return 0;
  }
  
  #if DEBUG > 99
    //Human Readable
    print_rssi_debug(pkt_reciever, pkt_source, pkt_rssi, pkt_id);
  #else
    //CSV Output
    print_rssi_csv(pkt_reciever, pkt_source, pkt_rssi, pkt_id);
  #endif  
  
  return 1;
}


inline void print_rssi_debug( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id ){
  uint8_t* buf = &print_buffer[0];
  
  uart_write( "[", 1 );
  hex_to_string( print_buffer, &pkt_reciever, 1 );
  uart_write( print_buffer, 2 );
  uart_write( "] ", 2 );
   
  uart_write( "Pkt From: ", 10 );
  hex_to_string( print_buffer, pkt_source, 1 );
  uart_write( print_buffer, 2 );
  
  uart_write( " PID: ", 6 );
  hex_to_string( print_buffer, &pkt_id, 1 );
  uart_write( print_buffer, 2 );
  
  uart_write( " RSSI: 0x", 9 );
  hex_to_string( print_buffer, pkt_rssi, 1 );
  uart_write( print_buffer, 2 );
 
  uart_write( " RSSI: ", 7 );
  //RSSI of beacon seen by AP
  buf += rssi_to_string( buf, pkt_rssi, 1 );
  *buf++ = ' ';
  *buf++ = 'd';
  *buf++ = 'B';
  *buf++ = 'm';
  *buf++ = '\n';
  //Flush buffer
  uart_write( print_buffer, buf - print_buffer );
}


inline void print_rssi_csv( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id ){
  uint8_t* buf = &print_buffer[0];
  
  //Packet ID
  buf += hex_to_string( buf, &pkt_id, 1 );
  *buf++ = ',';

  //AP which recv beacon from wban
  buf += hex_to_string( buf, pkt_source, 1 );
  *buf++ = ',';
  
  //RSSI of beacon seen by AP
  buf += rssi_to_string( buf, pkt_rssi, 1 );
  *buf++ = '\n';
  
  //Flush buffer
  uart_write( print_buffer, buf - print_buffer );
}

/*******************************************************************************
 * @fn     uint8_t fake_button_press()
 * @brief  Instead of using buttons, this function is called from a timer isr
 * ****************************************************************************/
uint8_t fake_button_press()
{
  buttonPressed = 1;
  
  //setup timer to trigger ~once per second
#if DEVICE_ADDRESS < 0x0A
  //AP - Vibe Dev
  increment_ccr(TOTAL_CCRS, 32768);
#else
  //WBAN - SimpVibe1
  set_ccr(TOTAL_CCRS, 32768);
#endif
  
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
#if DEVICE_ADDRESS < 0x0A
  //AP - Vibe Dev
  led3_toggle();
#else
  //WBAN - SimpVibe1
  led2_toggle();
#endif  
}


inline void signal_tx(){
#if DEVICE_ADDRESS < 0x0A
  //AP - Vibe Dev
  led1_toggle();
#else
  //WBAN - SimpVibe1
  led3_toggle();
#endif
}


inline void signal_yellow(){
#if DEVICE_ADDRESS < 0x0A
  //AP - Vibe Dev
  led2_toggle();
#else
  //WBAN - SimpVibe1
  led1_toggle();
#endif  
}


int main( void )
{
  uint8_t j;
  packet_data_t* tx_data;
  packet_header_t* header;

  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  
  header = (packet_header_t*)tx_buffer;
  tx_data = (packet_data_t*)(tx_buffer + sizeof(packet_header_t));
  
  // Initialize Tx Buffer
  header->length = PACKET_LEN;
  header->source = DEVICE_ADDRESS;
#if DEVICE_ADDRESS < 0x0A
  header->type = 0xAA;
  header->flags = 0x55;
#else
  header->type = 0xAB;
  header->flags = 0x55;
#endif
  
  // Fill in dummy values for the buffer
  for( j=0; j < TOTAL_SAMPLES; j++ )
  {
    //tx_data->samples[j] = j;
    tx_data->samples[j] = 0;
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
      process_tx();
      buttonPressed = 0;
    }
  }
  
  return 0;
}
