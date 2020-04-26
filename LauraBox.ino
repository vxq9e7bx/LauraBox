//
// Need to change this in ".arduino15/packages/esp32/hardware/esp32/1.0.4/tools/sdk/include/config/sdkconfig.h":
// #define CONFIG_ULP_COPROC_RESERVE_MEM 2048
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

// Create wifi-key.h, put the following two definitions in and replace the **** with your WIFI credentials.
//String ssid =     "****";
//String password = "****";
#include "wifi-key.h"

#include "Button.h"

// Pins for RC522 - need to be accessible by RTC / ULP coprocessor
#define GPIO_NFC_SS   GPIO_NUM_4    // GPIO 4, RTC GPIO 10
#define GPIO_NFC_RST  GPIO_NUM_14   // GPIO 14, RTC GPIO 16
#define GPIO_NFC_MOSI GPIO_NUM_32   // GPIO 32, RTC GPIO 9
#define GPIO_NFC_MISO GPIO_NUM_13   // GPIO 13, RTC GPIO 14
#define GPIO_NFC_SCK  GPIO_NUM_33   // GPIO 33, RTC GPIO 8

// Pins for SD card
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18

// Pins for I2S audio interface
#define I2S_DOUT      25
#define I2S_SCK       27
#define I2S_LRCK      26

// Pin for power control (SD card + audio)
#define POWER_CTRL    21

// Define push buttons (with pin numbers)
Button volumeUp{22};
Button volumeDown{15};    // suppresses boot messages if low => acceptable side effect
Button trackNext{16};
Button trackPrev{17};

Audio audio;
SPIClass sdspi(VSPI);

std::thread audioLoop;

size_t failCount{0};
std::vector<String> playlist;
size_t track{0};

hw_timer_t *timer{nullptr};

uint32_t active_card_id{0};
bool isPaused{false};
uint32_t pauseCounter{0};

bool doVolumeUp{false};
bool doVolumeDown{false};
size_t repeatCounter{0};
size_t currentVolume{10};

void IRAM_ATTR onTimer();

/**************************************************************************************************************/

void powerOff() {
  Serial.printf("Entering deep sleep\n\n");
  // inform ULP we are going to sleep
  ulp_main_cpu_sleeps = 1;
  // enable wakeup by ULP
  ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
  Serial.flush();
  esp_deep_sleep_start();
  // this never returns, the ESP32 is reset after deep sleep, hence setup() is called after wakeup
}

/**************************************************************************************************************/

static void init_ulp_program(bool firstBoot) {
  esp_err_t err;
  if(firstBoot) {
    /* Load ULP binary */
    err = ulptool_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

    /* Set ULP wake up period to 2000ms */
    ulp_set_wakeup_period(0, 2000 * 1000);
  
    /* Start the ULP program */
    ESP_ERROR_CHECK( ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t)));
  }

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

}

/**************************************************************************************************************/

uint32_t readCardIdFromULP() {
  uint32_t id;

  // spin lock
  while(true) {
    ulp_active_card_mutex_by_main_cpu = 1;
    if(! (ulp_active_card_mutex_by_ulp_cpu & 0xFFFF)) break;
    ulp_active_card_mutex_by_main_cpu = 0;
    delay(1);
  }
  
  id = ((ulp_active_card_id_hi & 0xFFFF) << 16) | (ulp_active_card_id_lo & 0xFFFF);

  // release lock
  ulp_active_card_mutex_by_main_cpu = 0;

  return id;
}

/**************************************************************************************************************/

void setup() {
  Serial.begin(115200);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_ULP) {
    Serial.printf("Cold start.\n");
    init_ulp_program(true);
    powerOff();
  }
  Serial.printf("ULP wakeup.\n");

  // inform ULP we are awake
  ulp_main_cpu_sleeps = 0;

  // init ULP pins
  init_ulp_program(false);

  // configure power control pin
  Serial.printf("Power on SD+Audio\n");
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, HIGH);

  // Setup SD card
  Serial.printf("Setup SD\n");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SD.begin(SD_CS);

  // Setup timer for scanning the input buttons
  Serial.printf("Setup input timer\n");
  timer = timerBegin(0, 80, true);      // prescaler 1/80 -> count microseconds
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);  // alarm every 10000 microseconds -> 100 Hz
  timerAlarmEnable(timer);

  // Setup input pins for push buttons
  Serial.printf("Setup input buttons\n");
  for (auto b : buttonList) b->setup();

  // Configure audo interface
  Serial.printf("Setup audio\n");
  audio.setPinout(I2S_SCK, I2S_LRCK, I2S_DOUT);
  audio.setVolume(currentVolume); // 0...21

  Serial.printf("Init complete.\n");
}

/**************************************************************************************************************/

void voiceError(String error) {
  Serial.print("Error: ");
  Serial.println(error);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  while(WiFi.status() != WL_CONNECTED) delay(500);

  audio.connecttospeech(error + " h.", "DE");
  powerOff();
}

/**************************************************************************************************************/

void IRAM_ATTR onTimer() {
  for(auto b : buttonList) b->onTimer();
  if(isPaused) ++pauseCounter;
}

/**************************************************************************************************************/

void loop() {

  // Check if card lost or changed. Will also be executed right after wakup from ULP.
  if(active_card_id != readCardIdFromULP()) {
    if(readCardIdFromULP() == 0) {
      if(!isPaused) {
        Serial.println("Card lost.");
        audio.pause();
        pauseCounter = 0;
        isPaused = true;
      }
      if(pauseCounter > 300*100) {
        Serial.println("Pausing timed out, powering down...");
        powerOff();
      }
      audio.loop();
      return;
    }

    active_card_id = readCardIdFromULP();
    String id = String(active_card_id, HEX);
    Serial.print("Card detected: ");
    Serial.print(id);
    Serial.println(".");      
  
    // Read playlist from SD card
    auto f = SD.open("/" + id + ".lst", FILE_READ);
    if(!f) {
      voiceError("Unbekannte Karte.");
    }
    while(f.available()) {
      playlist.push_back(f.readStringUntil('\n'));
    }
    f.close();
  
    // Start playing first track
    track = 0;
    audio.connecttoSD(playlist[track]);
  }
  // Card is same as currently playing one but we are paused: resume playback
  else if(isPaused) {
    Serial.println("Resume.");
    audio.resume();
  }

  // Check for button commands
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

  // Continue playing audio
  audio.loop();
}

/**************************************************************************************************************/

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

/**************************************************************************************************************/

void audio_info(const char *info) {
  Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info) { //id3 metadata
  Serial.print("id3data     "); Serial.println(info);
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

/**************************************************************************************************************/
