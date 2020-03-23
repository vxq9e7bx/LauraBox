#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#include <SPI.h>
#include "MFRC522.h"

#define NFC_SS  7
#define NFC_RST 8
#define NFC_PWR 6

class LowPowerHelper {
  LowPowerHelper() {
    // configure all pins as inputs with pullup - this is the recommendation by atmel for unused pins to save power
    // need to do this before the constructor of the nfc object etc.
    for(int i=0; i<22; ++i) pinMode(i,INPUT_PULLUP);
  }
};

MFRC522 nfc(NFC_SS, NFC_RST);

EMPTY_INTERRUPT(WDT_vect);

void setup() {
    
    // put your setup code here, to run once:
    Serial.begin(115200);

    // setup watchdog to wake up from sleep
    MCUSR  &= ~(1<<WDRF);             // Clear WDT reset flag
    WDTCSR |= (1<<WDCE) | (1<<WDE);   // Set WDCE
    WDTCSR  = (1<<WDP0) | (1<<WDP1) | (1<<WDP2);  // Prescaler to 2 seconds
    WDTCSR |= 1 << WDIE;              // Enable WDT interrupt
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  

    // disable ADC
    ADCSRA = 0;  
    power_adc_disable();

    // full power-down doesn't respond to Timer 2

    pinMode(NFC_RST, OUTPUT);
    pinMode(NFC_SS, OUTPUT);
    pinMode(NFC_PWR, OUTPUT);
    digitalWrite(NFC_PWR, HIGH);
    digitalWrite(NFC_SS, LOW);
    digitalWrite(NFC_RST, LOW);
    delay(250);
    digitalWrite(NFC_RST, HIGH);
    delay(250);
  
    nfc.begin();
    byte version = nfc.getFirmwareVersion();
    if(!version || version == 0xFF) {
      Serial.println("Didn't find MFRC522 board.");
    }
    Serial.print("Found chip MFRC522 ");
    Serial.print("Firmware ver. 0x");
    Serial.print(version, HEX);
    Serial.println(".");
    delay(5); // wait until serial buffer is clear
}

void loop() {
    // power down RC522
    digitalWrite(NFC_RST, LOW);

    // power down MCU (until Watchdog)
    //power_all_disable();
    sleep_enable();
    sleep_mode();
    sleep_disable();
    //power_spi_enable();
    Serial.println("!");

    // reinitialise RC522
    digitalWrite(NFC_RST, HIGH);
    nfc.begin();

    // Wait for RFID tag
    byte data[MAX_LEN];
    auto status = nfc.requestTag(MF1_REQIDL, data);
  
    if(status == MI_OK) {
      // read serial
      status = nfc.antiCollision(data);
      byte serial[4];
      memcpy(&serial, data, 4);
      String id = String(serial[0],HEX) + String(serial[1],HEX) + String(serial[2],HEX) + String(serial[3],HEX);

      Serial.print("Tag detected: ");
      Serial.print(id);
      Serial.println(".");
      delay(5); // wait until serial buffer is clear
    }
}
