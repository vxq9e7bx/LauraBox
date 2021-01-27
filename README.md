# LauraBox
A children's juke box controlled by RFID tags

<img src="https://raw.githubusercontent.com/mhier/LauraBox/master/images/complete_box.jpg" width="50%" alt="The ready box" align="right"/>

## Features:
* Playback of user-defined MP3 playlists
* Playlist selected though RFID tag
* Four buttons: volume +/-, next/previous track
* Intuitive pause functionality by removing the RFID tag and putting it back in place to resume
* Battery powered, can play while charging
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
* SD card to store MP3 files and playlists

## Firmware

The firmware is based on Arduino and uses the following libraries:
* ESP32 I2S audio library: https://github.com/schreibfaul1/ESP32-audioI2S
* ArduinoOTA (from Arduino library manager)
* ArduinoHttpClient (from Arduino library manager)

It also uses the ulptool (https://github.com/duff2013/ulptool) to program the ULP (ultra low power co-processor) of the ESP32. The ULP periodically queries the status of the RFID card, wakes the main CPU if a new card is detected, and informs the main CPU of the current card ID and when the card is lost.

Please have a look at the header of the Arduino sketch for instructions how to build the firmware.

## Electronics

<img src="https://raw.githubusercontent.com/mhier/LauraBox/master/images/mainboard_top.jpg" width="20%" alt="Mainboard top view" align="right"/>
<img src="https://raw.githubusercontent.com/mhier/LauraBox/master/images/mainboard_bottom.jpg" width="20%" alt="Mainboard bottom view" align="right"/>

* Analogue part is well-separated from digital part.
* Some things may be a bit cheated: digital signals (I2S etc.) going to the analogue side should better be decoupled with opto-couplers. It seems to work well like it is, though.
* Some components (choke, analogue supply bypass capacitor) are quite overdimensioned, just to be on the safe side.
* I am not an electronics engineer, surely one can do this better!

<p align="center">
<img src="https://raw.githubusercontent.com/mhier/LauraBox/master/images/schematics.svg" width="80%" alt="Schematics"/>
</p>

## Mechanical parts

<img src="https://raw.githubusercontent.com/mhier/LauraBox/master/images/inside.jpg" width="30%" alt="View inside opened box with mainboard removed" align="right"/>

* The outer parts are made from plywood with a CNC mill.
* The buttons are also made from wood, which creates a uniform look.
* The wood is finished with wood wax.
* The holder for the battery and the charge controller are 3D printed.
* The corners holding the electronics have nuts glued in from the bottom.
* Spacers are required between the electronics and the top lid of the casing. Assembly is slightly tedious, but thanks to Wifi connectivity this needs not be done often.
* The drilled holes for the sound to exit the box are too small, this degrades sound quality a bit unfortunately.

## Server setup for playlists and MP3 files

* A web server needs to be setup which provides playlists and MP3 files

TODO: describe how to do that
