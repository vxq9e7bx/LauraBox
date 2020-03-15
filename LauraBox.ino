#include <thread>
#include "Arduino.h"
#include "WiFi.h"
#include "src/Audio.h"
#include "SD.h"
#include "FS.h"
#include <SPI.h>

#include "src/MFRC522.h"

// Create wifi-key.h, put the following two definitions in and replace the **** with your WIFI credentials.
//String ssid =     "****";
//String password = "****";
#include "wifi-key.h"

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
struct Button;
std::vector<Button*> buttonList;

struct Button {
    explicit Button(int pin)
    : _pin(pin) {
      buttonList.push_back(this);
    }

    void setup() {
      pinMode(_pin, INPUT_PULLUP);
    }

    bool isCommandPending() {
      portENTER_CRITICAL_ISR(&timerMux);
      bool ret = _commandPending;
      _commandPending = false;
      portEXIT_CRITICAL_ISR(&timerMux);
      return ret;
    }

    void onTimer() {
      if(digitalRead(_pin) == HIGH) {
        // button not pressed
        _pressed = false;
        return;
      }

      if(_pressed) {
        ++_repeatCount;
        if(_repeatCount < 50) return;
      }
      _repeatCount = 0;
      _pressed = true;
    
      portENTER_CRITICAL_ISR(&timerMux);
      _commandPending = true;
      portEXIT_CRITICAL_ISR(&timerMux);
    }

  private:
    int _pin;
    bool _commandPending{false};
    bool _pressed{false};
    size_t _repeatCount{0};
};

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

// Define push buttons (with pin numbers)
Button volumeUp{32};
Button volumeDown{33};
Button trackNext{16};
Button trackPrev{17};
Button pausePlay{34};    // requires external pull up

Audio audio;
MFRC522 nfc(HSPI, NFC_SCK, NFC_MISO, NFC_MOSI, NFC_SS, NFC_RST);
SPIClass sdspi(VSPI);

std::thread audioLoop;

bool isPlaying{false};
size_t failCount{0};
std::vector<String> playlist;
size_t track{0};

hw_timer_t *timer{nullptr};


bool doVolumeUp{false};
bool doVolumeDown{false};
size_t repeatCounter{0};
size_t currentVolume{5};

void IRAM_ATTR onTimer();

void setup() {
    // disable watchdog (no longer necessary?)
    //disableCore0WDT();
    //disableCore1WDT();

    // Setup SD card
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    sdspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.begin(115200);
    SD.begin(SD_CS);

    // Setup MFRC522 and print firmware version
    Serial.println("Looking for MFRC522.");
    nfc.begin();
    byte version = nfc.getFirmwareVersion();
    if(!version || version == 0xFF) {
      Serial.print("Didn't find MFRC522 board.");
      while(1); //halt
    }
    Serial.print("Found chip MFRC522 ");
    Serial.print("Firmware ver. 0x");
    Serial.print(version, HEX);
    Serial.println(".");

    // Setup timer for scanning the input buttons
    timer = timerBegin(0, 80, true);      // prescaler 1/80 -> count microseconds
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 10000, true);  // alarm every 10000 microseconds -> 100 Hz
    timerAlarmEnable(timer);

    // Setup input pins for push buttons
    for(auto b : buttonList) b->setup();

    // Note: we initialise the I2S audio interface only when the music starts, because it interferes with the RFID interface (unclear why and how)
}

void voiceError(String error) {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while(WiFi.status() != WL_CONNECTED) delay(1500);
    
    audio.connecttospeech(error+" h.", "DE");
    while(1) audio.loop(); //halt
}

void IRAM_ATTR onTimer() {
    for(auto b : buttonList) b->onTimer();
}

void loop() {

    // RFID tag was already found, so play music
    if(isPlaying) {

      if(volumeUp.isCommandPending()) {
        if(currentVolume < 21) currentVolume++;
        audio.setVolume(currentVolume);
        Serial.print("Volume up: ");
        Serial.println(currentVolume);
      }
      if(volumeDown.isCommandPending()) {
        if(currentVolume > 0) currentVolume--;
        audio.setVolume(currentVolume);
        Serial.print("Volume down: ");
        Serial.println(currentVolume);
      }
      
      audio.loop();
      return;
    }
    
    // Wait for RFID tag
    byte data[MAX_LEN];
    auto status = nfc.requestTag(MF1_REQIDL, data);
  
    if(status == MI_OK) {
      failCount = 0;
      // read serial
      status = nfc.antiCollision(data);
      byte serial[4];
      memcpy(&serial, data, 4);
      String id = String(serial[0],HEX) + String(serial[1],HEX) + String(serial[2],HEX) + String(serial[3],HEX);

      Serial.print("Tag detected: ");
      Serial.print(id);
      Serial.println(".");
      isPlaying = true;

      audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
      audio.setVolume(currentVolume); // 0...21

      auto f = SD.open("/"+id+".lst", FILE_READ);
      if(!f) {
        voiceError("Unbekannte Karte.");
      }
      while(f.available()) {
        playlist.push_back(f.readStringUntil('\n'));
      }
      f.close();

      track = 0;
      audio.connecttoSD(playlist[track]);

      // wait until audio is really playing
      while(audio.getAudioFileDuration() == 0) audio.loop();
    }
    else {
      ++failCount;
      delay(100);
      if(failCount < 100) return;   // 10 seconds
      Serial.println("No tag found.");
      voiceError("Keine Karte gefunden.");
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

    // switch to next track of playlist
    ++track;
    if(track >= playlist.size()) {
      // if no track left: terminate
      Serial.println("Playlist has ended.");
      while(1);  //halt
    }
    audio.connecttoSD(playlist[track]);
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
