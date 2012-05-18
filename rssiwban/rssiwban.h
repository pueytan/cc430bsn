/** @file rssiwban.c
*
* @brief  Beacon sender for rssitest; simulates wban node
*
* @author Puey Wei Tan
* @author Alvaro Prieto
*/

#include "common.h"
#include "intrinsics.h"
#include "leds.h"
#include "timers.h"
#include <signal.h>
#include "oscillator.h"
#include "radio.h"

#define DEVICE_ADDRESS 0xA

#define TOTAL_SAMPLES (50)
#define RADIO_NUM_APS (2)
#define PACKET_ID_MAX (255)

uint8_t tx_buffer[PACKET_LEN+1];
volatile uint8_t timerCalled = 0;
uint8_t packet_id_tx = 0;

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
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
uint8_t process_rx( uint8_t* buffer, uint8_t size );

/*******************************************************************************
 * @fn     uint8_t process_rx( uint8_t* buffer, uint8_t size )
 * @brief  callback function called when new message is received
 * ****************************************************************************/
inline void process_tx( uint8_t* tx_data );

inline void signal_rx();
inline void signal_tx();
inline void signal_yellow();
uint8_t timer_callback (void);
int main( void );


