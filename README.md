# fesseeElectronique

La fessée électronique / The electronic spank.
   Spank machine like high striker/strong man game in funfair.
   Calculate velocity m/s, G impact and "clac" level of a spank.
   Florent Galès 2019 / Licence rien à branler / Do what the fuck you want licence.

   BOM:
     Arduino Nano.
     ADXL375 / GY-291 Accelerometer. Mix of diffenrent libraries like adafruit adxl345 (calculation) & https://github.com/georgeredinger/ADXL375_Testing/tree/master/ADXL375
     WS2812B led strip. Adafruit standard lib.
     Arcade switch button.
     8 digit 7 segment display. MAX7219 (SPI) : LedControl lib http://wayoda.github.io/LedControl/
     4 x (8 x 8 led display) for score / high score. MAX7219.LedControl lib http://wayoda.github.io/LedControl/
     Electret microphone
   
    PINS:
     ADXL 375 -> Arduino (I2C)
      GND -> GND
      VCC -> 3.3V
      SDA -> A4 / 4.75k resistor on 3.3v between A4 & SDA.
      SCL -> A5 / 4.75k resistor on 3.3v between A5 & SCL
      VS -> 3.3V
      SDO -> GND
      CS -> 3.3V
      INT1 -> 3

   Segement -> Arduino
     VCC -> 5V
     GND -> GND
     DIN -> 12 ??
     CS -> 8
     CLK -> 11 ??

   Led Matrix display (4 module 8x8) -> Arduino
    VCC -> 5V
     GND -> GND
     DIN -> 12 ??
     CS -> 9
     CLK -> 11 ??
