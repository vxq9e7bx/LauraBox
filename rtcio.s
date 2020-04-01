#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "stack.s"
#include "mfrc522_constants.h"

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

  .global stack
stack:
  .skip 8
  .global stackEnd
stackEnd:
  .long 0

  .global counter
counter:
  .long 0

  .global card_id_lo
card_id_lo:
  .long 0
  .global card_id_hi
card_id_hi:
  .long 0

  .data

  .global init_sequence
init_sequence:
  .byte TModeReg
  .byte 0x8D
  .skip 2
  .byte TPrescalerReg
  .byte 0x3E
  .skip 2
  .byte TReloadRegL
  .byte 0x30
  .skip 2
  .byte TReloadRegH
  .byte 0x00
  .skip 2
  .byte TxAutoReg
  .byte 0x40
  .skip 2
  .byte ModeReg
  .byte 0x3D
  .skip 2
  .byte TxControlReg
  .byte 0x83
  .skip 2
  .byte CommIEnReg
  .byte 0xF7
  .skip 2
  .byte FIFOLevelReg
  .byte 0x80  // flush fifo, really needed?
  .skip 2
  .byte FIFODataReg
  .byte MF1_REQIDL
  .skip 2
  .byte CommandReg
  .byte MFRC522_TRANSCEIVE
  .skip 2
  .byte BitFramingReg
  .byte 0x87
  .skip 2
  .global init_sequence_length
init_sequence_length:
  .long (init_sequence_length - init_sequence)/4

  .global getid_sequence
getid_sequence:
  .byte FIFOLevelReg
  .byte 0x80  // flush fifo
  .skip 2
  .byte FIFODataReg
  .byte MF1_ANTICOLL
  .skip 2
  .byte FIFODataReg
  .byte 0x20
  .skip 2
  .byte CommandReg
  .byte MFRC522_TRANSCEIVE
  .skip 2
  .byte BitFramingReg
  .byte 0x80
  .skip 2
  .global getid_sequence_length
getid_sequence_length:
  .long (getid_sequence_length - getid_sequence)/4

  /* Code goes into .text section */
  .text

  .global entry
entry:
  // initialise stack pointer
  move r3,stackEnd
   
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

  // init sequence loop
  assign counter, 0
init_sequence_loop:

  fetch r1, counter
  fetch_array r1, init_sequence, r1
  move r2, r1
  rsh r2, r2, 8
  call SPI_SET

  increment counter, 1
  fetch r1, counter
  fetch r2, init_sequence_length
  sub r0, r2, r1
  jumpr init_sequence_loop, 0, GT

  // wait for card detection
  assign counter, 0
detection_loop:

  wait 8000  // 1ms

  move r1, CommIrqReg
  call SPI_GET

  move r1, 0x20  // RxIRq bit set
  and r0, r1, r2
  jumpr card_detected, 0, GT

  increment counter, 1
  fetch r0, counter
  jumpr detection_loop, 25, LT

  jump sleep_ulp

card_detected:

  // get id sequence loop
  assign counter, 0
getid_sequence_loop:

  fetch r1, counter
  fetch_array r1, getid_sequence, r1
  move r2, r1
  rsh r2, r2, 8
  call SPI_SET

  increment counter, 1
  fetch r1, counter
  fetch r2, getid_sequence_length
  sub r0, r2, r1
  jumpr getid_sequence_loop, 0, GT

  // wait for id received
  assign counter, 0
getid_receive_loop:

  wait 8000  // 1ms

  move r1, CommIrqReg
  call SPI_GET

  move r1, 0x20  // RxIRq bit set
  and r0, r1, r2
  jumpr id_received, 0, GT

  increment counter, 1
  fetch r0, counter
  jumpr getid_receive_loop, 25, LT

  jump sleep_ulp

id_received:

  move r1, FIFODataReg
  call SPI_GET
  lsh r2, r2, 8
  assign card_id_hi, r2

  move r1, FIFODataReg
  call SPI_GET
  fetch r1, card_id_hi
  or r2, r1, r2
  assign card_id_hi, r2

  move r1, FIFODataReg
  call SPI_GET
  lsh r2, r2, 8
  assign card_id_lo, r2

  move r1, FIFODataReg
  call SPI_GET
  fetch r1, card_id_lo
  or r2, r1, r2
  assign card_id_lo, r2


wake_cpu:
  READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
  and r0, r0, 1
  jump wake_cpu, eq    // Retry until the bit is set
  wake

sleep_ulp:

  // disable MFRC522
  CLEAR_PIN(PIN_RST)

  /* Enable hold of pins */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,1)   // GPIO 4, RTC GPIO 10
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,1)   // GPIO 2, RTC GPIO 12
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD3_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,1)   // GPIO 15, RTC GPIO 13
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD4_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,1)   // GPIO 13, RTC GPIO 14
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD5_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,1)   // GPIO 12, RTC GPIO 15

  // sleep ULP program until next timer event
  halt


/*
 * Pseudo function: SPI_SET - set RC522 register through SPI
 *
 * r0 = internally used
 * r1 = register address
 * r2 = value to set
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
  ret


/*
 * Pseudo function: SPI_GET - get RC522 register through SPI
 *
 * r0 = internally used
 * r1 = register address
 * r2 = value is returned here
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
  CLEAR_PIN(PIN_SCK)
  lsh r2, r2, 1
  SET_PIN(PIN_SCK)
  GET_PIN(PIN_MISO)
  or r2, r2, r0
  stage_inc 1
  jumps spiGet_receiveLoop, 8, lt

  // end operation
  SET_PIN(PIN_SS)
  SET_PIN(PIN_MOSI)
  wait(5)

  // return
  ret
