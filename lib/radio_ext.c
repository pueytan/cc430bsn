#include "radio_ext.h"

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
		    uint8_t* pkt_rssi, uint8_t pkt_id, uint8_t pkt_grp ){
  //Disable printing if high numbered device (no uart available)
  if ( DEVICE_ADDRESS > 9 ){
    return 0;
  }
  
  #if DEBUG > 99
    //Human Readable
    print_rssi_debug(pkt_reciever, pkt_source, pkt_rssi, pkt_id, pkt_grp);
  #else
    //CSV Output
    print_rssi_csv(pkt_reciever, pkt_source, pkt_rssi, pkt_id, pkt_grp);
  #endif  
  
  return 1;
}


inline void print_rssi_debug( uint8_t pkt_reciever, uint8_t* pkt_source, 
		    uint8_t* pkt_rssi, uint8_t pkt_id, uint8_t pkt_grp ){
  uint8_t* buf = &print_buffer[0];
  
  uart_write( "[", 1 );
  hex_to_string( print_buffer, &pkt_reciever, 1 );
  uart_write( print_buffer, 2 );
  uart_write( "] ", 2 );
   
  uart_write( "Pkt From: ", 10 );
  hex_to_string( print_buffer, pkt_source, 1 );
  uart_write( print_buffer, 2 );
  
  uart_write( " PID: ", 6 );
  hex_to_string( print_buffer, &pkt_grp, 1 );
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
		    uint8_t* pkt_rssi, uint8_t pkt_id, uint8_t pkt_grp ){
  uint8_t* buf = &print_buffer[0];
  
  //Packet ID
  buf += hex_to_string( buf, &pkt_grp, 1 );
  buf += hex_to_string( buf, &pkt_id, 1 );
  *buf++ = ',';

  //AP which recv beacon from wban
  buf += hex_to_string( buf, &pkt_reciever, 1 );
  *buf++ = ',';
  
  //RSSI of beacon seen by AP
  buf += rssi_to_string( buf, pkt_rssi, 1 );
  *buf++ = '\n';
  
  //Flush buffer
  uart_write( print_buffer, buf - print_buffer );
}


void print_rx_debug(uint8_t* buffer, uint8_t size, packet_header_t* header, 
			     packet_footer_t* footer){
  // Print incoming packet information for debugging
  uart_write( "Size: ", 6 );
  hex_to_string( print_buffer, &header->length, 1 );
  uart_write( print_buffer, 2 );
  uart_write( " ", 1 );

  uart_write( " Type: ", 7 );
  hex_to_string( print_buffer, &header->type, 1 );
  uart_write( print_buffer, 2 );
  uart_write( " ", 1 );
  
  uart_write( " Flags: ", 8 );
  hex_to_string( print_buffer, &header->flags, 1 );
  uart_write( print_buffer, 2 );
  uart_write( " ", 1 ); 

  //Print whole packet in hex
  hex_to_string( print_buffer, buffer, size );
  uart_write( print_buffer, (size)*2 );
  uart_write( "\r\n", 2 );
}
