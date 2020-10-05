//
// Need to change this in ".arduino15/packages/esp32/hardware/esp32/1.0.4/tools/sdk/include/config/sdkconfig.h":
// #define CONFIG_ULP_COPROC_RESERVE_MEM 2048
//
// Need to install EPS32-audioI2S from here (download as ZIP, add via Sketch/Include Library)
// https://github.com/schreibfaul1/ESP32-audioI2S/
//

#include <thread>
#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include <SPI.h>
#include "SPIFFS.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>

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
//String updateCard = "<cardIdToActivateWifiUpload>";
//String baseUrl = "http://url.to.download.new.card.content"
#include "wifi-key.h"

#include "Button.h"

// Pins for RC522 - need to be accessible by RTC / ULP coprocessor
#define GPIO_NFC_SDA  GPIO_NUM_4    // GPIO 4, RTC GPIO 10
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

// Pin for power control (DAC and amplifier)
#define POWER_CTRL    21

// Pin for amplifier /shutdown and /mute signals
#define AMP_ENA       2   // needs to be pulled low for fw download -> put to programmer connector to be safe

// ADC channel to read the battery voltage. Keep in sync with constant in rtcio.s
#define ADC_CH_VBATT ADC1_CHANNEL_0

// Define push buttons (with pin numbers)
Button volumeUp{15};
Button volumeDown{22};    // suppresses boot messages if low => acceptable side effect
Button trackNext{17};
Button trackPrev{16};

Audio audio;
SPIClass sdspi(VSPI);

std::thread audioLoop;

size_t failCount{0};
std::vector<String> playlist;
size_t track{0};

hw_timer_t *timer{nullptr};

uint32_t active_card_id{0};
bool isStreaming{false};

void IRAM_ATTR onTimer();

bool isUpdateMode{false};

/**************************************************************************************************************/

void setupOTA() {
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
  
  
      SPIFFS.end();
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    })
    .setHostname("LauraBox")
    .setPort(3232);

    connectWifi();
    isUpdateMode = true;
    ArduinoOTA.begin();
    Serial.println("Update mode enabled.");
}

/**************************************************************************************************************/

void powerOff() {
  Serial.printf("Entering deep sleep\n\n");
  // shutdown SD card
  sdspi.end();
  digitalWrite(SD_CS, HIGH);
  sdspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SD.begin(SD_CS);
  
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

    /* Set ULP wake up period to 2 seconds */
    ulp_set_wakeup_period(0, 2 * 1000 * 1000);

    /* Slow alternative ULP wake up period in case of low battery: 10 minutes */
    ulp_set_wakeup_period(1, 60 * 1000 * 1000);  // * 10
  
    /* Start the ULP program */
    ESP_ERROR_CHECK( ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t)));

    /* Set default volume */
    ulp_current_volume = 8;

    /* Clear last card id */
    ulp_last_card_id = 0;
  }

  adc1_config_channel_atten(ADC_CH_VBATT, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_ulp_enable();

  err = rtc_gpio_init(GPIO_NFC_SDA);
  if (err != ESP_OK) Serial.printf("GPIO_NFC_SDA not ok for RTC\n");
  rtc_gpio_set_direction(GPIO_NFC_SDA, RTC_GPIO_MODE_OUTPUT_ONLY);

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
    audio.loop();
  }
  
  id = ((ulp_active_card_id_hi & 0xFFFF) << 16) | (ulp_active_card_id_lo & 0xFFFF);

  // release lock
  ulp_active_card_mutex_by_main_cpu = 0;

  return id;
}

/**************************************************************************************************************/

void setup() {
  Serial.begin(115200);

  // configure power control pin
  Serial.printf("Power on SD+Audio\n");
  pinMode(POWER_CTRL, OUTPUT);
  pinMode(AMP_ENA, OUTPUT);
  digitalWrite(AMP_ENA, LOW);
  digitalWrite(POWER_CTRL, HIGH);

  // Configure audo interface
  Serial.printf("Setup audio\n");
  audio.setPinout(I2S_SCK, I2S_LRCK, I2S_DOUT);

  // Setup SD card (done always to make sure SD card is in sleep mode after reset)
  Serial.printf("Setup SD\n");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  if(!SD.begin(SD_CS)) {
    voiceError("error");
  }

  // Check wake cause
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_ULP) {
    Serial.printf("Cold start.\n");
    init_ulp_program(true);
    powerOff();
  }
  Serial.printf("ULP wakeup.\n");
  Serial.print("VBATT: ");
  Serial.println(ulp_vbatt & 0xFFFF);

  // inform ULP we are awake
  ulp_main_cpu_sleeps = 0;

  // init ULP pins
  init_ulp_program(false);

  // set audio volume
  audio.setVolume(ulp_current_volume); // 0...21
  Serial.print("Volume: ");
  Serial.println(ulp_current_volume);

  // Setup timer for scanning the input buttons
  Serial.printf("Setup input timer\n");
  timer = timerBegin(0, 80, true);      // prescaler 1/80 -> count microseconds
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);  // alarm every 10000 microseconds -> 100 Hz
  timerAlarmEnable(timer);

  // Setup input pins for push buttons
  Serial.printf("Setup input buttons\n");
  for (auto b : buttonList) b->setup();

  Serial.printf("Enable amplifier\n");
  delay(500);
  digitalWrite(AMP_ENA, HIGH);

  Serial.printf("Init complete.\n");
}

/**************************************************************************************************************/

void voiceError(String error) {
  Serial.print("Error: ");
  Serial.println(error);

  digitalWrite(AMP_ENA, HIGH);

  if(!SPIFFS.begin()){
    Serial.println("SPIFFS Mount Failed");
    powerOff();
  }

  audio.stopSong();
  while(audio.isRunning()) audio.loop();
  audio.loop();

  audio.setVolume(8);

  playlist.clear();
  audio.connecttoFS(SPIFFS, error+".mp3");
  audio.loop();
  while(audio.isRunning()) audio.loop();

  powerOff();
}

/**************************************************************************************************************/

void IRAM_ATTR onTimer() {
  for(auto b : buttonList) b->onTimer();
}

/**************************************************************************************************************/

void connectWifi() {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting WIFI...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    size_t cTimeOut = 0;
    while(WiFi.status() != WL_CONNECTED) {
      if(++cTimeOut > 60) {
        voiceError("error");
      }
      delay(500);
    }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

/**************************************************************************************************************/

void play(String uri) {
  if(!uri.startsWith("http://") && !uri.startsWith("https://")) {
    isStreaming = false;
    WiFi.disconnect();
    audio.connecttoSD(uri);
  }
  else {
    isStreaming = true;
    connectWifi();
    audio.connecttohost(uri);
  }
}

/**************************************************************************************************************/

void loop() {

  // Check if low battery
  if(ulp_vbatt_low & 0xFFFF) {
    voiceError("charge");
  }

  // Handle update mode
  if(isUpdateMode) {
    ArduinoOTA.handle();
  }

  // Check if card lost or changed. Will also be executed right after wakup from ULP.
  auto detected_card_id = readCardIdFromULP();
  if(active_card_id != detected_card_id) {
    if(detected_card_id == 0) {
      Serial.println("Card lost.");
      if(audio.isRunning()) audio.pauseResume();

      // store current position in RTC memory, to allow resuming if same card is read again
      ulp_last_card_id = active_card_id;
      ulp_last_track = track;
      if(!isStreaming) {
        ulp_last_file_position = audio.getFilePos();
      }
      else {
        ulp_last_file_position = 0;
      }
      powerOff();
    }

    active_card_id = detected_card_id;
    String id = String(active_card_id, HEX);
    Serial.print("Card detected: ");
    Serial.print(id);
    Serial.println(".");

    // check for special update card
    if(id == updateCard) {
      setupOTA();
      return;
    }

    // Read playlist from SD card
    auto f = SD.open("/" + id + ".lst", FILE_READ);
    if(!f) {
      connectWifi();
      HTTPClient http;
      auto url = baseUrl+"/"+id+".lst";
      Serial.println("Attempting url: "+url);
      http.begin(url);
      int httpCode = http.GET();
      if(httpCode != 200) {
        Serial.print("Http error: ");
        Serial.println(httpCode);
        voiceError("unknown_id");
      }
      Serial.println("Found online, starting download...");
      // Download playlist
      auto f = SD.open("/" + id + ".lst", FILE_WRITE);
      http.writeToStream(&f);
      f.close();
      http.end();
      // Download each track on playlist
      f = SD.open("/" + id + ".lst", FILE_READ);
      while(f.available()) {
        auto track = f.readStringUntil('\n');
        Serial.println("Downloading '"+track+"'...");
        http.begin(baseUrl+"/"+track);
        int httpCode = http.GET();
        if(httpCode != 200) {
          voiceError("error");
        }
        auto f = SD.open("/"+track, FILE_WRITE);
        http.writeToStream(&f);
        f.close();
        http.end();
      }
      f.seek(0);
    }
    while(f.available()) {
      playlist.push_back(f.readStringUntil('\n'));
    }
    f.close();

    // Start playing first track
    if(ulp_last_card_id != active_card_id) {
      track = 0;
      play(playlist[track]);
    }
    else {
      Serial.println("Resuming.");
      track = ulp_last_track;
      play(playlist[track]);
      audio.setFilePos(ulp_last_file_position);
    }
  }

  // Check for button commands
  if(volumeUp.isCommandPending()) {
    if(ulp_current_volume < 21) ulp_current_volume++;
    audio.setVolume(ulp_current_volume);
    Serial.print("Volume up: ");
    Serial.println(ulp_current_volume);
  }
  if(volumeDown.isCommandPending()) {
    if(ulp_current_volume > 0) ulp_current_volume--;
    audio.setVolume(ulp_current_volume);
    Serial.print("Volume down: ");
    Serial.println(ulp_current_volume);
  }
  if(trackNext.isCommandPending()) {
    if(track < playlist.size()-1) {
      ++track;
      play(playlist[track]);
    }
  }
  if(trackPrev.isCommandPending()) {
    if(track > 0) {
      --track;
      play(playlist[track]);
    }
  }

  // Continue playing audio
  audio.loop();
}

/**************************************************************************************************************/

void audio_eof_mp3(const char *info) { //end of file
  Serial.print("eof_mp3     "); Serial.println(info);

  // switch to next track of playlist
  ++track;
  if(track >= playlist.size()) {
    // if no track left: terminate
    Serial.println("Playlist has ended.");
    active_card_id = 0;
    powerOff();
  }
  play(playlist[track]);
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
