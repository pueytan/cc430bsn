/** @file access_point.c
*
* @brief Network Discovery Access Point
*   
* The main idea for this application is to discover all end devices (EDs) on the
* network and characterize all the links between them. Once all links have been
* characterized, new routing methods can be used.
* 
* Initially, the access point (AP) will begin by sending a beacon at full power.
* After receiving the start beacon, each ED will wait an ammount of time
* proportional to its network address (to avoid collisions) and reply with an
* acknowledge packet.
*
* The acknowledge packet will be a broadcast message so that all other EDs can 
* 'listen in' and record the RSSI.
*
* Once all EDs have replied, the AP will poll each ED for its received power
* table. This table will include a device address and received RSSI for all
* devices it was able to receive messages from.
*
* The AP will then send the collected data to the host computer for processing.
* (Eventually, all processing will be done on the AP itself, but for now, it 
* will offline.)
*
* @author Alvaro Prieto
*/
#include "common.h"
#include "intrinsics.h"
#include "leds.h"
#include "uart.h"
#include "timers.h"
#include "cc2500.h"
#include "radio_cc2500.h"
#include "network.h"

#include <string.h>

#define DEVICE_ADDRESS (MAX_DEVICES + 1)

#define FIRST_COMMAND '0'

#define print(x) uart_write( x, strlen(x) )


// These are the different states the device can be in
#define STATE_WAIT        (0x00)
#define PROCESS_PACKET    (0x01)
#define PROCESS_COMMAND   (0x02)


static uint8_t sync_message (void);
static uint8_t blink_led1 (void);
static uint8_t rx_callback( uint8_t*, uint8_t );
static uint8_t run_serial_command( uint8_t );

static void poll_ed( uint8_t ed_address );

static void command_0(void);
static void command_1(void);
static void command_2(void);
static void command_null(void);

static void (* const serial_commands[])(void) = { command_0, command_1,
                                                  command_2, command_null };
static volatile uint8_t counter = LED_BLINK_CYCLES;
static volatile uint8_t send_sync_message = 0;

static uint8_t p_radio_tx_buffer[RADIO_BUFFER_SIZE];
static uint8_t p_radio_rx_buffer[RADIO_BUFFER_SIZE];

static volatile uint8_t rssi_table[MAX_DEVICES+1][MAX_DEVICES+1];
static volatile uint8_t device_table[MAX_DEVICES+1];

static volatile uint8_t current_device;

static volatile uint8_t current_state = STATE_WAIT;
static volatile uint8_t last_packet_size;
static volatile uint8_t last_serial_command;

static volatile uint8_t processing_packet = 0;

static uint8_t sync_packet[] = {0x03, 0x00, DEVICE_ADDRESS, PACKET_SYNC};

// DEBUG used for measuring poll response time
uint16_t time_start, time_end, time_poll;

// DEBUG for printing statements
uint8_t string[100];

int main( void )
{   
  uint8_t sleep = 1;
  /* Init watchdog timer to off */
  WDTCTL = WDTPW|WDTHOLD;
     
  // Setup oscillator for 12MHz operation
  BCSCTL1 = CALBC1_12MHZ;
  DCOCTL = CALDCO_12MHZ;
 
  // Wait for changes to take effect
  __delay_cycles(4000);
      
  setup_uart();
  
  setup_uart_callback( run_serial_command );
  
  setup_spi();
  
  setup_timer_a(MODE_CONTINUOUS);
  
  register_timer_callback( blink_led1, 1 );
  register_timer_callback( sync_message, 0 );

  set_ccr( 1, 32768 );
  
  set_ccr( 0, 0 );
  
  setup_cc2500( rx_callback );
  
  cc2500_set_address( DEVICE_ADDRESS );
       
  setup_leds();
  
  // Initialize all devices to 'disconnected'
  memset( (void*)device_table, 0x00, sizeof(device_table) );  
           
  print("\r\nACCESS POINT\r\n");
 
  for (;;) 
  {        
    switch ( current_state )
    {
      case STATE_WAIT:
      {    
        print("\r\n\nEnter command from ");
        uart_put_char( FIRST_COMMAND );
      
        print(" to ");
        uart_put_char( FIRST_COMMAND + sizeof(serial_commands)/2 - 1 );
        print(":\r\n");
        
        sleep = 1;
        
        break;
      }
      
      case PROCESS_PACKET:
      {
        packet_header_t* rx_packet;
        
        dint();
        processing_packet = 1;
        eint();
                   
        rx_packet = (packet_header_t*)p_radio_rx_buffer;
        
        if ( (PACKET_START | FLAG_ACK) == rx_packet->type )
        { 
          
          // Make sure source is within bounds
          if ( ( rx_packet->source <= ( MAX_DEVICES ) )  
                                   && ( rx_packet->source > 0) )
          {
            // Store RSSI in table
            rssi_table[AP_INDEX][rx_packet->source] = 
                                            p_radio_rx_buffer[last_packet_size];
            
            // Since the device replied to the poll, we can assume it is 'active'
            device_table[rx_packet->source] |= DEVICE_ACTIVE;
            
            string[0] = rx_packet->source + '0';
            string[1] = 0;
            print("saf:"); // start ack from
            print(string);
            print("\r\n");
            
          }
          else
          {
            print("dob\r\n"); // device out of bounds
          }
        
          sleep = 1;
        }
         
        if ( (PACKET_POLL | FLAG_ACK) == rx_packet->type )
        {

          // Make sure source is within bounds
          if ( ( rx_packet->source <= ( MAX_DEVICES ) )  
                                   && ( rx_packet->source > 0) )
          {
            string[0] = rx_packet->source + '0';
            string[1] = 0;
            print(string);
            print("\r\n");

          
            // Copy RSSI table from device to master table
            //TODO
            
            // Check if there are any more devices to be polled
            while( ( current_device < MAX_DEVICES ) 
                && !( device_table[current_device] & DEVICE_POLL_SCHEDULED ) )
            {
              /*print("Checking device ");
              string[0] = current_device + '0';
              string[1] = 0;
              print(string);
              print("\r\n"); */
              current_device++;
            }           

            if ( device_table[current_device] & DEVICE_POLL_SCHEDULED )
            {
              __delay_cycles(100);
              poll_ed(current_device);
              sleep = 1;
            }
            else
            {
              
              print("done polling\r\n"); // done polling devices
            }

            led2_off();
          }

        }
        
        current_state = STATE_WAIT;
        
        memset( p_radio_rx_buffer, 0x00, sizeof(last_packet_size) );
        
        dint();
        processing_packet = 0;
        eint();
        
        
        break;
      }
      
      case PROCESS_COMMAND:
      {
        
        serial_commands[last_serial_command - FIRST_COMMAND]();
        
        sleep = 1;
        current_state = STATE_WAIT;
        
        break;
      }
      
      default:
      {
        print("dft\r\n");
        current_state = STATE_WAIT;
      }
    }
    
    if( sleep )
    {
      sleep = 0;
      __bis_SR_register( LPM1_bits + GIE );   // Sleep
    }
    
  } 
  
  return 0;
}

/*******************************************************************************
 * @fn     uint8_t blink_led1( void )
 * @brief  timer_a callback function, blinks LED every LED_BLINK_CYCLES
 * ****************************************************************************/
static uint8_t blink_led1 (void)
{
  
  if( 0 == counter-- )
  {
    //led1_toggle();
    counter = LED_BLINK_CYCLES;
    send_sync_message = 1;
  }
  
  return 0;
}

/*******************************************************************************
 * @fn     uint8_t send_sync_message( void )
 * @brief  Send sync message to keep ED clocks aligned
 * ****************************************************************************/
static uint8_t sync_message (void)
{
  if (send_sync_message)
  {
    
    cc2500_tx( sync_packet, 4 );
    send_sync_message = 0;
  }
  
  return 0;
}

/*******************************************************************************
 * @fn     void rx_callback( void )
 * @brief  rx callback function, called when packet has been received
 * ****************************************************************************/
static uint8_t rx_callback( uint8_t* p_buffer, uint8_t size )
{  
  if( processing_packet == 1 )
  {
    print("err pp\r\n");
  }

  memcpy( p_radio_rx_buffer, p_buffer, size );
  last_packet_size = size;
  current_state = PROCESS_PACKET;
  
  return 1;
}

/*******************************************************************************
 * @fn     void poll ed( uint8_t ed_address )
 * @brief  Send poll packet to ED with address ed_address
 * ****************************************************************************/
static void poll_ed( uint8_t ed_address )
{  
  packet_header_t* poll_packet;
          
  poll_packet = (packet_header_t*)p_radio_tx_buffer;

  // Initialize POLL packet
  poll_packet->destination = ed_address;
  poll_packet->source = DEVICE_ADDRESS;
  poll_packet->type = PACKET_POLL;    
  
  // Send poll packet
  // cc2500_tx_packet already adds the destination field
  cc2500_tx_packet( &p_radio_tx_buffer[1], 2, poll_packet->destination );
  
  // Clear the poll sent flag
  // TODO: Switch this upon ack receipt to do multiple tries
  device_table[ed_address] &= ~DEVICE_POLL_SCHEDULED;
  
  led2_on();
  
  

}

/*******************************************************************************
 * @fn     void run_serial_command( uint8_t command )
 * @brief  Run serial commands...
 * ****************************************************************************/
static uint8_t run_serial_command( uint8_t command )
{
  
  // Check to be sure the command is in range
  if( ( command >= FIRST_COMMAND) && 
      ( command < ( FIRST_COMMAND + sizeof(serial_commands)/2 ) ) )
  {
    current_state = PROCESS_COMMAND;
    
    last_serial_command = command;
    
    return 1;
  }
  else
  {
    print("Invalid command!\r\n");
    return 0;
  }
  
  
  
  
}

/*******************************************************************************
 * @fn     void command_1( )
 * @brief  Send START packet to initialize network discovery
 * ****************************************************************************/
static void command_0(void)
{
  packet_header_t* start_packet;
  
  print("Starting network discovery...\r\n");
    
  start_packet = (packet_header_t*)p_radio_tx_buffer;
  
  // Initialize START packet
  start_packet->source = DEVICE_ADDRESS;
  start_packet->destination = BROADCAST_ADDRESS;
  start_packet->type = PACKET_START;
  
  // Send START packet
  cc2500_tx_packet( &p_radio_tx_buffer[1], 2, start_packet->destination );
    
  
      
}

/*******************************************************************************
 * @fn      void command_1(void)
 * @brief   Start polling devices
 * ****************************************************************************/
static void command_1(void)
{  
  uint8_t device_counter;
  
  print("Poll\r\n");
  
  for( device_counter = MAX_DEVICES; device_counter > 0 ; device_counter-- )
  {
    if( device_table[device_counter] && DEVICE_ACTIVE )
    {
      // If the device is active, schedule a poll message
      device_table[device_counter] |= DEVICE_POLL_SCHEDULED;
      
      // Determine which device to poll first
      current_device = device_counter;
      
      string[0] = device_counter + '0';
      string[1] = 0;
      print("sp "); // scheduled poll
      print(string);
      print("\r\n"); 
    }
  }
  
  // Send poll packet
  poll_ed(current_device);
}

/*******************************************************************************
 * @fn     void command_2( uint8_t command )
 * @brief  Print debug information
 * ****************************************************************************/
static void command_2(void)
{
  
  print("Device table: ");  
  hex_to_string( string, (uint8_t*)device_table, sizeof(device_table) );
  print("0x");
  print( string );
  print("\r\n");

}

/*******************************************************************************
 * @fn     void command_null(void)
 * @brief  pointer for commands that have not been implemented
 * ****************************************************************************/
static void command_null(void)
{
  // Start network discovery
  print("This command has not been implemented yet\r\n");
}
