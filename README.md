# La fessée électronique

   La fessée électronique / The electronic spank.<br>
   Spank machine like high striker/strong man game in funfair.<br>
   Calculate velocity m/s, G impact and "clac" level of a spank.<br>
   Score led strip animation inspired by bouncing balls effect => https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/<br>
   
   Florent Galès 2019 / Licence rien à branler / Do what the fuck you want licence.<br>

## BOM:
     Arduino Nano.
     ADXL375 / GY-291 Accelerometer. Mix of diffenrent libraries like adafruit adxl345 (calculation) & https://github.com/georgeredinger/ADXL375_Testing/tree/master/ADXL375
     Paddle for spanking, hide adxl375 and electret inside. https://fr.aliexpress.com/item/32825780214.html
     You can found a 3d printed enclosure to put the adxl375 at the back of the paddle. Inside the handle it give bad results. The electret must be placed at the bottom of the handle to avoid shock.
     WS2812B led strip (one for score and one for clac db level with 10 led). Adafruit standard lib.
     Arcade switch button.
     8 digit 7 segment display. MAX7219 (SPI) : https://github.com/bartoszbielawski/LEDMatrixDriver
     4 x (8 x 8 led display) for score / high score. MAX7219.https://github.com/bartoszbielawski/LEDMatrixDriver
     Electret microphone
     Servo: use TiCoServo to work with neopixel. 
   
## PINS:
```
ADXL 375 -> Arduino (I2C)
      GND (marron) -> GND
      VCC (orange) -> 3.3V
      SDA (vert) -> A4 / 4.75k resistor on 3.3v between A4 & SDA.
      SCL (bleu) -> A5 / 4.75k resistor on 3.3v between A5 & SCL
      VS -> 3.3V
      SDO -> GND
      CS -> 3.3V
      INT1 -> 3
```
```
Segment -> Arduino
     VCC -> 5V
     GND -> GND
     DIN -> 12
     CS -> 8
     CLK -> 13
```
```
Led Matrix display (4 module 8x8) -> Arduino
	VCC -> 5V
     GND -> GND
     DIN -> 12
     CS -> 9
     CLK -> 13
```
```
Strip WS2812B for score (30led/m) -> Arduino
	VCC -> 5V
	GND -> GND
	DATA -> D6
```
```
Strip WS2812B for "clac" level (30led/m) -> Arduino
	VCC -> 5V
	GND -> GND
	DATA -> D5
```
```
Arcade button -> Arduino
	pin 1 -> GND
	pin 2 -> D4
```
```
Mic: A3 - 22kOhm between A3 and 5V.
```
```
Servo for bell: D10 -> Use this pin or D9 for working with neopixel.
```
