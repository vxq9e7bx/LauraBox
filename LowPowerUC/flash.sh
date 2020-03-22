sudo avrdude -c linuxspi -p m328p -P /dev/spidev0.0 -Uflash:w:$1 -V -v
