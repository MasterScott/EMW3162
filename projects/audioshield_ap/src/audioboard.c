/*
 * audioboard.c
 *
 *  Created on: Jan 10, 2015
 *      Author: wouters
 */
#include "wiced.h"

#include <stm32f2xx.h>
#include <stm32f2xx_i2c.h>
#include <stm32f2xx_rcc.h>
#include <stm32f2xx_gpio.h>
#include "audioboard.h"
#include "spi.h"
#include "wiced_platform.h"
#define ADDR 0x34 // i2c address and write bit
#define SDA_PIN GPIO_Pin_7 // i2c data line
#define SCL_PIN GPIO_Pin_6 // i2c clock line
#define LED_PIN 13 // board led pin

#define HIGH Bit_SET
#define LOW Bit_RESET


void digitalWrite(int pin, int value) {
	GPIO_WriteBit(GPIOB, pin, value);
}

int digitalRead(int pin) {
	return GPIO_ReadOutputDataBit(GPIOB, pin);
}
// us delay timer (~1us per unit)
// sets the i2c clock rate

void mydelay(long t) {
	long i = 0;



	for(i = 0; i < (7 * t); i++) {
		__asm("nop");
	}
}


// i2c start condition
char i2cbb_start(void) {
  if (digitalRead(SDA_PIN) == 0) { // check if data line released
    return -1; // end with failure if not released
  }

  else { // send start condition
    digitalWrite(SDA_PIN, LOW);  // data low
    mydelay(10); // delay
    //WPRINT_PLATFORM_INFO( ("Start..\n") );
    digitalWrite(SCL_PIN, LOW); // clock low
    mydelay(10); // delay
    return 1; // set state to success
  }
}


// i2c stop condition
void i2cbb_stop(void) {
  digitalWrite(SDA_PIN, LOW); // pull data low
  mydelay(10); // delay
  digitalWrite(SCL_PIN, HIGH); // release clock line
  mydelay(10); // delay
  digitalWrite(SDA_PIN, HIGH); // release data line
  mydelay(400); // delay to make sure a new data transfer doesnt occur too quickly
}


// i2c data send
char i2cbb_send(unsigned char data) {  // clock out data
  unsigned char state = 0; // initialize return state
  unsigned char i;
  for(i = 8 ; i > 0  ; i--) {
    digitalWrite(SDA_PIN, (data & (1 << (i - 1))));
    mydelay(10);
    digitalWrite(SCL_PIN, HIGH);
    mydelay(10);
    digitalWrite(SCL_PIN, LOW);
  }
  // check for ack
  digitalWrite(SDA_PIN, LOW); // release line

  mydelay(9); // wait a bit
// // clock the ack or nack
  digitalWrite(SCL_PIN, HIGH);
  mydelay(9);
  digitalWrite(SCL_PIN, LOW);
  // make sure line is released
  mydelay(15);

  if (state > 1) { // send stop if failure
    i2cbb_stop();
  }
  else { // set state to success
    state = 1;
  }
  return state;
}

// full i2c protocol for 3 byte transfer
unsigned char i2cbb(unsigned char reg, unsigned char data) {
  if (i2cbb_start() != 1) { // send start condition
    return 2;
  }
  else if (i2cbb_send(ADDR) != 1) { // send address and write bit
    return 3;
  }
  else if (i2cbb_send(reg) != 1) { // send register to write to
    return 4;
  }
  else if (i2cbb_send(data) != 1) { // write data to register
    return 5;
  }
  else {
  i2cbb_stop(); // send stop condition
  return 1;
  }
}

void codec_maple_reg_setup(void) {
  i2cbb(0x0c, 0x00); // power save registers -> all on
	  mydelay(10);

  i2cbb(0x0e, 0x03); // digital data format -> 16b spi mode
	  mydelay(10);

  i2cbb(0x00, LINVOL); // left input configure
	  mydelay(10);

  i2cbb(0x02, RINVOL); // right input configure
	  mydelay(10);

  i2cbb(0x04, LHPVOL); // left headphone configure
	  mydelay(10);

  i2cbb(0x06, RHPVOL); // right headphone configure
	  mydelay(10);

  i2cbb(0x0a, ADCHPD);  // digital pathway configure
	  mydelay(10);

  i2cbb(0x08, (SIDEATT << 6)|(SIDETONE << 5)|(DACSEL << 4)|(BYPASS << 3)|(INSEL << 2)|(MUTEMIC << 1)|(MICBOOST << 0)) ; // analog pathway configure
	  mydelay(10);


  #if SAMPLE_RATE == 88
    while (i2cbb(0x10, 0xbc) ; // clock select 88.2kHz
    	mydelay(10);
    }
  #elif SAMPLE_RATE == 44
   i2cbb(0x10, 0xa0);// clock select 44.1kHz
    	mydelay(10);

  #elif SAMPLE_RATE == 22
    i2cbb(0x10, 0xe0) ; // clock select 22.05kHz
    	mydelay(10);

  #elif SAMPLE_RATE == 8
    i2cbb(0x10, 0xac); // clock select 8.018kHz
    	mydelay(10);

  #elif SAMPLE_RATE == 2
     i2cbb(0x10, 0xce) ; // clock select 2.45kHz
    	mydelay(10);

  #endif

   i2cbb(0x12, 0x01) ;// device enable
	  mydelay(10);

}

int spi_send(platform_spi_message_segment_t* segments, uint16_t num_segments) {
	if ( WICED_SUCCESS != wiced_spi_transfer( &wiced_spi_audioshield, (wiced_spi_message_segment_t*) segments, (uint16_t) num_segments ) )
	{
		return -1;
	}
	return 0;
}
static wiced_result_t send_data( void *arg )
{
	const void*   tx_buffer = "test";
	void*         rx_buffer;
	unsigned long length = 1;

	platform_spi_message_segment_t segments[1] =
	{
			{ tx_buffer,            rx_buffer,       length }
	};
	if(spi_send(segments, 1) != 0) {
		return WICED_ERROR;
	}
return WICED_SUCCESS;

}


static wiced_timed_event_t spi_data_event;
void I2C_LowLevel_Init()
{

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	   GPIO_InitTypeDef gpioStructure;
	   gpioStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	   gpioStructure.GPIO_Mode = GPIO_Mode_OUT;
	   gpioStructure.GPIO_PuPd = GPIO_PuPd_UP;
	   gpioStructure.GPIO_OType = GPIO_OType_PP;
	   gpioStructure.GPIO_Speed = GPIO_Speed_25MHz;
	   GPIO_DeInit(GPIOB);
	   GPIO_Init(GPIOB, &gpioStructure);

		//GPIO_WriteBit(GPIOB, GPIO_Pin_6, value);
   	   mydelay(500);

	   digitalWrite(SCL_PIN, HIGH); // release clock line
	   digitalWrite(SDA_PIN, HIGH); // release data line
   	   mydelay(500);

	  // digitalWrite(SCL_PIN, HIGH);
   	   codec_maple_reg_setup();
	   GPIO_DeInit(GPIOB);



	   if ( WICED_SUCCESS != wiced_spi_init( &wiced_spi_audioshield ) )
	   {
		   return ;
	   }
	   wiced_rtos_register_timed_event( &spi_data_event, WICED_HARDWARE_IO_WORKER_THREAD, &send_data, 1, 0 );

}

