#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

#define PIN_SS    10     // GPIO 4, RTC GPIO 10
#define PIN_RST   12     // GPIO 2, RTC GPIO 12
#define PIN_MOSI  13     // GPIO 15, RTC GPIO 13
#define PIN_MISO  14     // GPIO 13, RTC GPIO 14
#define PIN_SCK   15     // GPIO 12, RTC GPIO 15

#define SET_PIN(PIN) WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG,RTC_GPIO_OUT_DATA_W1TS_S+PIN,1,1)
#define CLEAR_PIN(PIN) WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG,RTC_GPIO_OUT_DATA_W1TC_S+PIN,1,1)
#define GET_PIN(PIN) READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + PIN, 1) 


  /* Define variables, which go into .bss section (zero-initialized data) */
  .bss

  .global card_id
card_id:
  .long 0

  /* Code goes into .text section */
  .text

  .global entry
entry:
  /* Disable hold of pins */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,0)   // GPIO 4, RTC GPIO 10
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG,RTC_IO_TOUCH_PAD2_HOLD_S,1,0)   // GPIO 2, RTC GPIO 12
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD3_REG,RTC_IO_TOUCH_PAD3_HOLD_S,1,0)   // GPIO 15, RTC GPIO 13
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD4_REG,RTC_IO_TOUCH_PAD4_HOLD_S,1,0)   // GPIO 13, RTC GPIO 14
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD5_REG,RTC_IO_TOUCH_PAD5_HOLD_S,1,0)   // GPIO 12, RTC GPIO 15

  .global loop
loop:

  // reset RC522 and initialise SPI
  CLEAR_PIN(PIN_RST)
  SET_PIN(PIN_SS)
  SET_PIN(PIN_SCK)
  SET_PIN(PIN_MOSI)
  wait 100
  SET_PIN(PIN_RST)

  // set 0x01 to 0x0F
  move r1, 0x01
  move r2, 0x0F
  move r3, ret01
  jump SPI_SET
ret01:

  // read 0x01
  move r1, 0x01
  move r3, ret02
  jump SPI_GET
ret02:

  // write 0x01
  move r1, 0x01
  move r3, ret02b
  jump SPI_SET
ret02b:

  // set 0x2A to 0x8D
  move r1, 0x2A
  move r2, 0x8D
  move r3, ret03
  jump SPI_SET
ret03:

  jump loop


/*
 * Pseudo function: SPI_SET - set RC522 register through SPI
 *
 * r0 = internally used
 * r1 = register address
 * r2 = value to set
 * r3 = return address
 * 
 * All register content is destroyed after return (apart from r3, including stage counter).
 */
  .global SPI_SET
SPI_SET:

  // start operation
  CLEAR_PIN(PIN_SS)

  // -> send first byte: the address
  // left-shift address by 1, see MFRC522 data sheet section 8.1.2.3 "SPI address byte"
  lsh r1, r1, 1

  // reset loop counter
  stage_rst
spiSet_SendLoop1:
  SET_PIN(PIN_SCK)
  and r0, r1, 0x80
  jumpr spiSet_setPin1, 0x80, eq
  CLEAR_PIN(PIN_MOSI)
  jump spiSet_cont1
spiSet_setPin1:
  SET_PIN(PIN_MOSI)
spiSet_cont1:
  CLEAR_PIN(PIN_SCK)
  lsh r1, r1, 1
  stage_inc 1
  jumps spiSet_SendLoop1, 8, lt

  // terminate send
  SET_PIN(PIN_SCK)
  wait(5)

  // -> send second byte: the value

  // reset loop counter
  stage_rst
spiSet_sendLoop2:
  SET_PIN(PIN_SCK)
  and r0, r2, 0x80
  jumpr spiSet_setPin2, 0x80, eq
  CLEAR_PIN(PIN_MOSI)
  jump spiSet_cont2
spiSet_setPin2:
  SET_PIN(PIN_MOSI)
spiSet_cont2:
  CLEAR_PIN(PIN_SCK)
  lsh r2, r2, 1
  stage_inc 1
  jumps spiSet_sendLoop2, 8, lt

  // end operation
  SET_PIN(PIN_SCK)
  SET_PIN(PIN_SS)
  SET_PIN(PIN_MOSI)
  wait(5)

  // return
  jump r3


/*
 * Pseudo function: SPI_GET - get RC522 register through SPI
 *
 * r0 = internally used
 * r1 = register address
 * r2 = value is returned here
 * r3 = return address
 * 
 * r2 containes read data, all other register content is destroyed (including stage counter, except r3)
 */
  .global SPI_GET
SPI_GET:

  // start operation
  CLEAR_PIN(PIN_SS)

  // -> send first byte: the address
  // left-shift address by 1, see MFRC522 data sheet section 8.1.2.3 "SPI address byte"
  lsh r1, r1, 1
  // set uppermost bit to make read request
  or r1, r1, 0x80

  // reset loop counter
  stage_rst
spiGet_sendLoop:
  SET_PIN(PIN_SCK)
  and r0, r1, 0x80
  jumpr spiGet_setPin, 0x80, eq
  CLEAR_PIN(PIN_MOSI)
  jump spiGet_cont
spiGet_setPin:
  SET_PIN(PIN_MOSI)
spiGet_cont:
  CLEAR_PIN(PIN_SCK)
  lsh r1, r1, 1
  stage_inc 1
  jumps spiGet_sendLoop, 8, lt

  // terminate send
  SET_PIN(PIN_SCK)
  wait(5)

  // -> receive second byte: the value
  CLEAR_PIN(PIN_MOSI)

  // reset loop counter
  move r2, 0
  stage_rst
spiGet_receiveLoop:
  SET_PIN(PIN_SCK)
  GET_PIN(PIN_MISO)
  or r2, r2, r0
  lsh r2, r2, 1
  CLEAR_PIN(PIN_SCK)
  stage_inc 1
  jumps spiGet_receiveLoop, 8, lt

  // end operation
  SET_PIN(PIN_SCK)
  SET_PIN(PIN_SS)
  SET_PIN(PIN_MOSI)
  wait(5)

  // return
  jump r3
