# Estimation of power consumption of low-power part

Measurement is done for the two states ON and SLEEP separately. To measure `on`, `nfc.requestTag` is executed in an endless loop. To measure `off`, the watchdog is disabled and hence the MPU never wakes up.

The measurement is performed with a 10 Ohms shunt. The supply voltage is set to 3.6 V. The supply voltage is measured excluding the shunt, so the measurement is free of any insertion loss.

The measurement includes only the ATMega 328P MCU and the RC522 RFID reader with the minimum circuit (just capacitors and the reset pull up). The power LED of the RC522 breakout has been removed.

Next, the duty cycle is measured for a watchdog period of 2 seconds.

## Result of current measurement

ON: 34 mA (a bit higher when a tag is in range)
SLEEP: 0.36 mA
Note: when removing the RC522 from the board in SLEEP, the current is below the measurement threshold (<0.1mA)

h2. Result of duty cycle measurement
