#include "Arduino.h"
#include "WiFi.h"
#include "src/Audio.h"
#include "SD.h"
#include "FS.h"
#include <SPI.h>

#include "src/MFRC522.h"

// Create wifi-key.h, put the following two definitions in and replace the **** with your WIFI settings.
//String ssid =     "****";
//String password = "****";
#include "wifi-key.h"

// Pins for RC522
#define NFC_SS        15
#define NFC_RST       22
#define NFC_MOSI      13
#define NFC_MISO      12
#define NFC_SCK       14

// Pins for SD card
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18

// Pins for I2S audio interface
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;
MFRC522 nfc(HSPI, NFC_SCK, NFC_MISO, NFC_MOSI, NFC_SS, NFC_RST);
SPIClass sdspi(VSPI);
 
void setup() {
    pinMode(SD_CS, OUTPUT);
    
    digitalWrite(SD_CS, HIGH);
    sdspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.begin(115200);
    SD.begin(SD_CS);

    // Init MFRC522 and print firmware version
    Serial.println("Looking for MFRC522.");
    nfc.begin();
    byte version = nfc.getFirmwareVersion();
    if (! version) {
      Serial.print("Didn't find MFRC522 board.");
      while(1); //halt
    }
    Serial.print("Found chip MFRC522 ");
    Serial.print("Firmware ver. 0x");
    Serial.print(version, HEX);
    Serial.println(".");

    //WiFi.mode(WIFI_OFF);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(5); // 0...21

    audio.connecttoSD("/03_Bach_BWV1052_Allegro.mp3");
    //audio.connecttohost("http://www.ndr.de/resources/metadaten/audio/m3u/ndrkultur.m3u");
//    audio.connecttohost("http://macslons-irish-pub-radio.com/media.asx");
//    audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.aac"); //  128k aac
//      audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.mp3"); //  128k mp3
    //audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");
//    audio.connecttohost("http://media.ndr.de/download/podcasts/podcast4161/AU-20190404-0844-1700.mp3"); // podcast
}

void loop()
{
    audio.loop();

    // Wait for RFID tag
    byte data[MAX_LEN];
    auto status = nfc.requestTag(MF1_REQIDL, data);
  
    if(status == MI_OK) {
      Serial.println("Tag detected.");
      Serial.print("Type: ");
      Serial.print(data[0], HEX);
      Serial.print(", ");
      Serial.println(data[1], HEX);
  
      // calculate the anti-collision value for the currently detected
      // tag and write the serial into the data array.
      status = nfc.antiCollision(data);
      byte serial[5];
      memcpy(serial, data, 5);
  
      Serial.println("The serial nb of the tag is:");
      for(size_t i = 0; i < 3; i++) {
        Serial.print(serial[i], HEX);
        Serial.print(", ");
      }
      Serial.println(serial[3], HEX);
    }

}

// optional
void audio_info(const char *info){
    Serial.print("info        ");Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreaminfo(const char *info){
    Serial.print("streaminfo  ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info){
    Serial.print("eof_speech  ");Serial.println(info);
}
