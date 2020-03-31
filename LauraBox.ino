//
// Need to change this in ".arduino15/packages/esp32/hardware/esp32/1.0.4/tools/sdk/include/config/sdkconfig.h":
// #define CONFIG_ULP_COPROC_RESERVE_MEM 1024
//

#include <thread>
#include "Arduino.h"
#include "WiFi.h"
#include "src/Audio.h"
#include "SD.h"
#include "FS.h"
#include <SPI.h>


#include "esp_deep_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/soc.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "ulp_main.h"
#include "ulptool.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");


#include "src/MFRC522.h"

// Create wifi-key.h, put the following two definitions in and replace the **** with your WIFI credentials.
//String ssid =     "****";
//String password = "****";
#include "wifi-key.h"

#include "Button.h"

// Pins for RC522 - need to be accessible by RTC / ULP coprocessor
#define NFC_SS        4
#define GPIO_NFC_SS   GPIO_NUM_4    // GPIO 4, RTC GPIO 10
#define NFC_RST       2
#define GPIO_NFC_RST  GPIO_NUM_2    // GPIO 2, RTC GPIO 12
#define NFC_MOSI      15
#define GPIO_NFC_MOSI GPIO_NUM_15   // GPIO 15, RTC GPIO 13
#define NFC_MISO      13
#define GPIO_NFC_MISO GPIO_NUM_13   // GPIO 13, RTC GPIO 14
#define NFC_SCK       12
#define GPIO_NFC_SCK  GPIO_NUM_12   // GPIO 12, RTC GPIO 15

// Pins for SD card
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18

// Pins for I2S audio interface
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

// Pin for power control (soft power off)
#define POWER_CTRL    21

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

void powerOff() {
  Serial.printf("Entering deep sleep\n\n");
  // enable wakeup by ULP
  ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
  Serial.flush();
  esp_deep_sleep_start();
  // this never returns, the ESP32 is reset after deep sleep, hence setup() is called after wakeup
}


static void init_ulp_program() {
  esp_err_t err = ulptool_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
  ESP_ERROR_CHECK(err);

  err = rtc_gpio_init(GPIO_NFC_SS);
  if (err != ESP_OK) Serial.printf("GPIO_NFC_SS not ok for RTC\n");
  rtc_gpio_set_direction(GPIO_NFC_SS, RTC_GPIO_MODE_OUTPUT_ONLY);

  err = rtc_gpio_init(GPIO_NFC_RST);
  if (err != ESP_OK) Serial.printf("GPIO_NFC_RST not ok for RTC\n");
  rtc_gpio_set_direction(GPIO_NFC_RST, RTC_GPIO_MODE_OUTPUT_ONLY);

  err = rtc_gpio_init(GPIO_NFC_MOSI);
  if (err != ESP_OK) Serial.printf("GPIO_NFC_MOSI not ok for RTC\n");
  rtc_gpio_set_direction(GPIO_NFC_MOSI, RTC_GPIO_MODE_OUTPUT_ONLY);

  err = rtc_gpio_init(GPIO_NFC_MISO);
  if (err != ESP_OK) Serial.printf("GPIO_NFC_MISO not ok for RTC\n");
  rtc_gpio_set_direction(GPIO_NFC_MISO, RTC_GPIO_MODE_INPUT_ONLY);

  err = rtc_gpio_init(GPIO_NFC_SCK);
  if (err != ESP_OK) Serial.printf("GPIO_NFC_SCK not ok for RTC\n");
  rtc_gpio_set_direction(GPIO_NFC_SCK, RTC_GPIO_MODE_OUTPUT_ONLY);

  /* Set ULP wake up period to 2000ms */
  ulp_set_wakeup_period(0, 2000 * 10000);

  /* Start the ULP program */
  ESP_ERROR_CHECK( ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t)));
}


void setup() {
  Serial.begin(115200);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_ULP) {
    Serial.printf("Not ULP wakeup, initializing ULP\n");
    init_ulp_program();
  } else {
    Serial.printf("ULP wakeup\n");
  }

  while (true) {
    Serial.printf("card id %x\n", ulp_card_id & 0xFFFF);
  }

  powerOff();


  // Setup MFRC522 and print firmware version
  Serial.println("Looking for MFRC522.");
  nfc.begin();
  /*    byte version = nfc.getFirmwareVersion();
      if(!version || version == 0xFF) {
        Serial.println("Didn't find MFRC522 board.");
        powerOff();
      }
      Serial.print("Found chip MFRC522 ");
      Serial.print("Firmware ver. 0x");
      Serial.print(version, HEX);
      Serial.println(".");
  */
  // Wait for RFID tag
  byte data[MAX_LEN];
  auto status = nfc.requestTag(MF1_REQIDL, data);
  if (status != MI_OK) {
    powerOff();
  }

  // configure power control pin
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, HIGH);

  // Setup SD card
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SD.begin(SD_CS);

  // Setup timer for scanning the input buttons
  timer = timerBegin(0, 80, true);      // prescaler 1/80 -> count microseconds
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);  // alarm every 10000 microseconds -> 100 Hz
  timerAlarmEnable(timer);

  // Setup input pins for push buttons
  for (auto b : buttonList) b->setup();

  // read serial
  status = nfc.antiCollision(data);
  byte serial[4];
  memcpy(&serial, data, 4);
  String id = String(serial[0], HEX) + String(serial[1], HEX) + String(serial[2], HEX) + String(serial[3], HEX);

  Serial.print("Tag detected: ");
  Serial.print(id);
  Serial.println(".");
  isPlaying = true;

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(currentVolume); // 0...21

  auto f = SD.open("/" + id + ".lst", FILE_READ);
  if (!f) {
    voiceError("Unbekannte Karte.");
  }
  while (f.available()) {
    playlist.push_back(f.readStringUntil('\n'));
  }
  f.close();

  track = 0;
  audio.connecttoSD(playlist[track]);

  // wait until audio is really playing
  while (audio.getAudioFileDuration() == 0) audio.loop();

}

void voiceError(String error) {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) delay(1500);

  audio.connecttospeech(error + " h.", "DE");
  powerOff();
}

void IRAM_ATTR onTimer() {
  for (auto b : buttonList) b->onTimer();
}

void loop() {

  // RFID tag was already found, so play music
  if (isPlaying) {

    if (volumeUp.isCommandPending()) {
      if (currentVolume < 21) currentVolume++;
      audio.setVolume(currentVolume);
      Serial.print("Volume up: ");
      Serial.println(currentVolume);
    }
    if (volumeDown.isCommandPending()) {
      if (currentVolume > 0) currentVolume--;
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

  if (status == MI_OK) {
    failCount = 0;
  }
  else {
    ++failCount;
    delay(100);
    if (failCount < 100) return;  // 10 seconds
    Serial.println("No tag found.");
    voiceError("Keine Karte gefunden.");
  }

}

// optional
void audio_info(const char *info) {
  Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info) { //id3 metadata
  Serial.print("id3data     "); Serial.println(info);
}
void audio_eof_mp3(const char *info) { //end of file
  Serial.print("eof_mp3     "); Serial.println(info);

  // switch to next track of playlist
  ++track;
  if (track >= playlist.size()) {
    // if no track left: terminate
    Serial.println("Playlist has ended.");
    powerOff();
  }
  audio.connecttoSD(playlist[track]);
}
void audio_showstation(const char *info) {
  Serial.print("station     "); Serial.println(info);
}
void audio_showstreaminfo(const char *info) {
  Serial.print("streaminfo  "); Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle "); Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate     "); Serial.println(info);
}
void audio_commercial(const char *info) { //duration in sec
  Serial.print("commercial  "); Serial.println(info);
}
void audio_icyurl(const char *info) { //homepage
  Serial.print("icyurl      "); Serial.println(info);
}
void audio_lasthost(const char *info) { //stream URL played
  Serial.print("lasthost    "); Serial.println(info);
}
void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  "); Serial.println(info);
}
