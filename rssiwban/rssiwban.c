/** @file rssiwban.c
*
* @brief  Beacon sender for rssitest; simulates wban node
*
* @author Puey Wei Tan
* @author Alvaro Prieto
*/
#include "rssiwban.h"


/*******************************************************************************
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
uint8_t process_rx( uint8_t* buffer, uint8_t size )
{
  return 1;
}


/*******************************************************************************
 * @fn     uint8_t process_tx( uint8_t* txdata )
 * @brief  Function called to transmit new packet
 * ****************************************************************************/
inline void process_tx( uint8_t* tx_data ){
  tx_data[4] = packet_id_tx++;		//Packet ID, sequential
  radio_tx( tx_buffer, sizeof(tx_buffer) );
  signal_tx();    			// Pulse LED during Transmit
}


inline void signal_rx(){
  //AP - Vibe Dev
  led3_toggle();
}


inline void signal_tx(){
  led1_toggle();
}


inline void signal_yellow(){
  led2_toggle();
}


/*******************************************************************************
 * @fn     uint8_t fake_button_press()
 * @brief  Timer isr. Releases the radio transmission
 * ****************************************************************************/
uint8_t timer_callback (void)
{
  timerCalled = 1;
  return 1;
}


int main( void )
{
  uint8_t j;
  uint8_t* tx_data;
  packet_header_t* header;
  
  // Turn off watchdog timer
  WDTCTL = WDTPW|WDTHOLD;
  
  header = (packet_header_t*)tx_buffer;
  tx_data = ( (packet_data_t*)(tx_buffer + sizeof(packet_header_t)) )->samples;
  
  // Initialize Tx headers
  header->length = PACKET_LEN;
  header->source = DEVICE_ADDRESS;
  header->type = 0xAB;
  header->flags = 0x55;
  
  // Fill in dummy values for the buffer
  for( j=0; j < TOTAL_SAMPLES; j++ ){
    tx_data[j] = 0;
  }
  
  // Make sure processor is running at 12MHz
  setup_oscillator();
    
  // Initialize LEDs
  setup_leds();

  setup_timer_a(MODE_UP);
  register_timer_callback( timer_callback, 0 );
  set_ccr(0, 32800);	// 1 second
  
  // Init radio at +10dBm; enable receive callback
  setup_radio_pwr( process_rx, PATABLE_VAL_10DBM );
  
  eint();

  tx_data[0] = 0x7A;	//Data preamble
  tx_data[1] = 3;	//Set bytes used to 3 bytes
  
  for(;;) {
    //__bis_SR_register(LPM1_bits);
   
    if (timerCalled){
      process_tx(tx_data); 
      timerCalled = 0;
    }
  }  
}


