#include "Arduino.h"
#include "WiFi.h"
#include "src/Audio.h"
#include "SD.h"
#include "FS.h"

#define SIZE_BUFFER     18
#define MAX_SIZE_BLOCK  16
#include <MFRC522.h> //library responsible for communicating with the module RFID-RC522
#include <SPI.h> //library responsible for communicating of SPI bus

// Pins for RC522
#define SS_PIN    21
#define RST_PIN   22

// Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;

//used in authentication
MFRC522::MIFARE_Key key;
//authentication return status code
MFRC522::StatusCode status;
// Defined pins to module RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); 

#include "wifi-key.h"
// Create wifi-key.h, put the following two definitions in and replace the **** with your WIFI settings.
//String ssid =     "****";
//String password = "****";

void setup() {
    pinMode(SD_CS, OUTPUT);
    
    //digitalWrite(SD_CS, HIGH);
    //SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.begin();
    Serial.begin(115200);
    //SD.begin(SD_CS);

    // Init MFRC522
    mfrc522.PCD_Init();

    //WiFi.mode(WIFI_OFF);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(5); // 0...21

    //audio.connecttoSD("/03_Bach_BWV1052_Allegro.mp3");
    //audio.connecttohost("http://www.ndr.de/resources/metadaten/audio/m3u/ndrkultur.m3u");
//    audio.connecttohost("http://macslons-irish-pub-radio.com/media.asx");
//    audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.aac"); //  128k aac
//      audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.mp3"); //  128k mp3
    //audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");
//    audio.connecttohost("http://media.ndr.de/download/podcasts/podcast4161/AU-20190404-0844-1700.mp3"); // podcast
}

void loop()
{
    //audio.loop();
    delay(500);

    // waiting the card approach
    if(!mfrc522.PICC_IsNewCardPresent()) {
      return;
    }
    // Select a card
    if(!mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Error reading serial");
      return;
    }
    unsigned long uid;
    uid =  mfrc522.uid.uidByte[0] << 24;
    uid += mfrc522.uid.uidByte[1] << 16;
    uid += mfrc522.uid.uidByte[2] <<  8;
    uid += mfrc522.uid.uidByte[3];
    //mfrc522.PICC_HaltA(); // Stop reading
    Serial.print("Card detected, UID: ");
    Serial.println(uid);
    // Dump debug info about the card; PICC_HaltA() is automatically called
    mfrc522.PICC_DumpToSerial(&(mfrc522.uid));    
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
