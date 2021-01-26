# LauraBox
A children's juke box controlled by RFID tags

## Features:
* Playback of user-defined MP3 playlists
* Playlist selected though RFID tag
* Four buttons: volume +/-, next/previous track
* Intuitive pause functionality by removing the RFID tag and putting it back in place to resume
* Battery powered
* WiFi download of now MP3s and playlists
* Firmware upgrade via WiFi
* Long standby time (several month)
* Long play time (around 20 hours)
* Toddler proof :-)

## Main components
* EPS32 microcontroller
* MFRC522 RFID board
* PCM5102 I2S stereo audio DAC
* PAM8403 stereo amplifier
* Pair of wideband speakers
* 3.6V Li-ion battery with USB charge controller from a USB power bank

## Firmware

The firmware is based on Arduino and uses the following libraries:
* ESP32 I2S audio library: https://github.com/schreibfaul1/ESP32-audioI2S
* ArduinoOTA (from Arduino library manager)
* ArduinoHttpClient (from Arduino library manager)

It also uses the ulptool (https://github.com/duff2013/ulptool) to program the ULP (ultra low power co-processor) of the ESP32. The ULP periodically queries the status of the RFID card, wakes the main CPU if a new card is detected, and informs the main CPU of the current card ID and when the card is lost.

Please have a look at the header of the Arduino sketch for instructions how to build the firmware.

##
