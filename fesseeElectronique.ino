/*
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
     DIN -> 12
     CS -> 8
     CLK -> 13

   Led Matrix display (4 module 8x8) -> Arduino
    VCC -> 5V
     GND -> GND
     DIN -> 12
     CS -> 9
     CLK -> 13
     
   Strip WS2812B for score (30led/m) -> Arduino
    VCC -> 5V
    GND -> GND
    DATA -> D6

   Arcade button -> Arduino
    pin 1 -> GND
    pin 2 -> D4
*/

#include "ADXL375.h"
#include <LEDMatrixDriver.hpp>
#include <Adafruit_NeoPixel.h>
#include "constants.h"

#define MIN_Z 10 // Min value to store data in fifo.
#define MIN_CLAC 100 // Minimum for a spank.
#define DROP_CLAC 30 // Min drop after a max event.
#define SCORE_BELL 1000 // A which score we ring the bell.

#define PIN_MIC A3

#define PIN_BTN 4

#define PIN_STRIP_SCORE 6
#define NBLED_STRIP_SCORE 60
Adafruit_NeoPixel stripScore = Adafruit_NeoPixel(NBLED_STRIP_SCORE, PIN_STRIP_SCORE, NEO_GRB + NEO_KHZ800);

#define PIN_STRIP_CLAC 5
#define NBLED_STRIP_CLAC 10
Adafruit_NeoPixel stripClac = Adafruit_NeoPixel(NBLED_STRIP_CLAC, PIN_STRIP_CLAC, NEO_GRB + NEO_KHZ800);

byte buff[6] ;    //6 bytes buffer for saving data read from the device

LEDMatrixDriver segment (1, 8);
LEDMatrixDriver ledMatrix(4, 9); 

float maxZ = 0;
char tabloScore[7] = {' ', ' ', ' ', ' ', ' ', ' ', ' '};
float fifo[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long lastClac = 0;

void setup(void)
{
  Serial.begin(9600);

  // Start button
  pinMode(PIN_BTN, INPUT_PULLUP);

  // Mic
  pinMode(PIN_MIC, INPUT);

  stripScore.begin();
  stripScore.clear();
  stripScore.show();
  stripClac.begin();
  stripClac.setBrightness(60);
  stripClac.clear();
  stripClac.show();
  
  segment.setEnabled(true);
  segment.setIntensity(15);  // 1= min, 15=max
  segment.setScanLimit(7);  // 0-7: Show 1-8 digits. Beware of currenct restrictions for 1-3 digits! See datasheet.
  segment.setDecode(0xFF);  // Enable "BCD Type B" decoding for all digits.
  clearSegment();
  //segment.setEnabled(false);
  
  ledMatrix.setEnabled(true);
  ledMatrix.setIntensity(5); // 1= min, 15=max
  ledMatrix.clear();
  ledMatrix.display();

  //displayMessage("AAAAAAAA", 10);
  //displayMessage("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 30);
  //displayScore(9999);
  while(digitalRead(PIN_BTN) == HIGH){
    // do nothing
  }
  /*showScoreAnim(999);
  showScoreAnim(800);
  showScoreAnim(500);
  showScoreAnim(100);
  showScoreAnim(1300);*/
  displayMs(213);
  displayG(10.89);
  displayClacDb(1000);
  displayMessage("READY!", 50);
  drawString(" GO!", 4, 0, 0);
  ledMatrix.display();

  // Adxl375 init
  //pinMode(3, INPUT);
  //attachInterrupt(digitalPinToInterrupt(3), shock, RISING); // Interruption if single shock (INT1)
  Wire.begin();        // join i2c bus (address optional for master)
  setupFIFO();
  readFrom(DEVICE, 0X30, 1, buff);

  
}

void loop(void)
{
  unsigned long currentMillis = millis();
  // Check si détection d'un choc sur l'axe Z
  //byte buff[1];
  //readFrom(DEVICE, ADXL375_ACT_TAP_STATUS_REG, 1, buff);
  //Serial.println(buff[1]);
  //Serial.println(analogRead(A3));
  //float x = getX() * ADXL345_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
  //float y = getY() * ADXL345_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
  float z = getZ() * ADXL375_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD; //<= format en mG/LSB (milli G par Last Significant Bit). ADXL375 = 49mg/LSB. Donc z en G = z * 0.049
  //Serial.println(z);
  if(z > MIN_Z && currentMillis > lastClac + 500){ // If last clac after 1/2 second.
    insertNewValue(z);
    //Détection du maxiumum
    if(z > maxZ){
      maxZ = z;
    }else { // Max reached we test a big drop
     if(z < maxZ - DROP_CLAC){
      if(maxZ > MIN_CLAC) { //We got a real spank!
        int mic = analogRead(A3);
        
        // Show fifo
        Serial.print("CLAC:");
        Serial.println(mic);
        for(byte i = 0; i < 10; i++){
          Serial.println(fifo[i]);
        }

        float gForce = maxZ / 9.8;
        float velocity = calcVelocity(maxZ);
        float score = (mic/10) * gForce * velocity;
        showScoreAnim(score);
        displayMs(velocity);
        displayG(gForce);
        displayClacDb(mic);
        displayScore(score);

        // wait for 5s and reset.
        delay(5000);
        clearFifo();
        
        maxZ = 0;
        lastClac = millis();
      } else { // false flag is not a "real" spank.
        clearFifo();
        maxZ = 0;
        lastClac = millis();
      }
     }
    }
  }
}

void shock(){
  Serial.println("choc");
}

void insertNewValue(float z){
  for(byte i = 9; i > 0; i--){
    fifo[i] = fifo[i - 1];
  }
  fifo[0] = z;
  //fifoCounter = fifoCounter > 10 ? 10 : fifoCounter++;
}

void clearFifo(){
  for(byte i = 0; i < 10; i++){
    fifo[i] = 0;
  }
  //fifoCounter = 0;
}

float calcVelocity(float maxZ){
  // Do not calculate with the impact and after.
  float velocity = 0;
  byte sum = 0;
  bool bBeforeClac = false;
  for(byte i = 1; i < 10; i++){
    if(bBeforeClac){
      velocity += fifo[i];
      sum++;
    }
    if(fifo[i] == maxZ){
      bBeforeClac = true; 
    }
  }
  velocity = velocity / sum;
  return velocity;
}


void showScoreAnim(float score) {
  score = score > SCORE_BELL ? SCORE_BELL : score;
  byte level = map(score, 0, SCORE_BELL, 0, NBLED_STRIP_SCORE);

  byte posAnim = 0;
  long lastAnimMillis = 0;
  float h ;     
  float hauteur = score / SCORE_BELL;// An array of heights
  float vImpact = sqrt( -2 * -1 * hauteur ); ;                   // As time goes on the impact velocity will change, so make an array to store those values
  float tCycle ;                    // The time since the last time the ball struck the ground
  int   pos ;                       // The integer position of the dot on the strip (LED index)
  long  tLast ;                     // The clock time of the last ground strike
  tLast = millis() - 1;
  h = hauteur;
  pos = 0;
  tCycle = 0;

  while (h > 0) {
    long currentMillis = millis();
    tCycle =  currentMillis - tLast ;     // Calculate the time since the last time the ball was on the ground
    // Calculate position with time, acceleration (gravity) and intial velocity
    h = 0.5 * -0.98 * pow( tCycle/1000 , 2.0 ) + vImpact * tCycle/1000;
    pos = round( h * (level - 1) / hauteur);       // Map hauteur to pixel pos
    stripScore.setPixelColor(pos, 255, 0, 0);
    stripScore.show();
    //Off for next loop.
    stripScore.setPixelColor(pos, 0, 0, 0);

    if(currentMillis - lastAnimMillis > 30 && posAnim < 33){
      lastAnimMillis = currentMillis;
      showClacAnim(posAnim);
      posAnim++;
    }
 }

  //Ring bell
  if(score > SCORE_BELL){
    ringBell();
  }

  // Finish matrix animation.
  while(posAnim < 33){
    long currentMillis = millis();
    if(currentMillis - lastAnimMillis > 30){
      lastAnimMillis = currentMillis;
      showClacAnim(posAnim);
      posAnim++;
    }
  }
}

void ringBell(){
  
}

void displayClacDb(int mic) {
  byte level = map(mic, 0, 1024, 0, 10);

  for(int i = 9; i >= 10 - level; i --){
    byte r = 0, g = 0, b = 0;
    if(i > 7){
      b = 255;
    } else if(i > 5){
      g = 255;
    } else if(i > 3){
      r = 255;
      g = 255;
    } else if(i > 1){
      r = 255;
      g = 69;
    } else {
      r = 255;
    }
    stripClac.setPixelColor(i, r, g, b);
  }
  stripClac.show(); 
}

void displayMs(float x) {
  //segment.setEnabled(true);
  unsigned int i = x > 999 ? 999 : x;
  char buf[4];
  sprintf(buf, "%04u", i);

  //segment.setDigit(7, i >= 1000 ? buf[0] : LEDMatrixDriver::BCD_BLANK, false);
  segment.setDigit(7, i >= 100 ? buf[1] : LEDMatrixDriver::BCD_BLANK, false);
  segment.setDigit(6, i >= 10 ? buf[2] : LEDMatrixDriver::BCD_BLANK, false);
  segment.setDigit(5, buf[3], false);
  segment.display();
}

void displayG(float x) {
  //segment.setEnabled(true);
  x = abs(x);
  char buf[10];
  sprintf(buf, "%f", x);
  dtostrf(x, 4, 2, buf);

  int digit = 3;
  for (int i = 0; i < 5; i++) {
    if (buf[i] != '.') {
      //segment.setChar(0, digit, buf[i], buf[i + 1] == '.');
      segment.setDigit(digit, buf[i], buf[i + 1] == '.');
      digit--;
    }
  }
  segment.display();
}

void displayScore(unsigned int score)
{
  Serial.println(score);
  if(score > 9999){
    score = 9999;
  }
  ledMatrix.clear();
  ledMatrix.display();

  if(score > 1000){
    unsigned int milleDigit = (score / 1000U) % 10;
    unsigned int milleScore = 0;
    char buf[4];
    for(int i = 1; i <= milleDigit ; i++){
      milleScore = i * 1000;
      sprintf(buf, "%04u", milleScore);
      drawString(buf, 4, 0, 0);
      ledMatrix.display();
      delay(300);
    }
    score = score - milleScore;
  }

  if(score > 100){
    unsigned int centDigit = (score / 100U) % 10;
    unsigned int centScore = 0;
    char buf[3];
    for(int i = 1; i <= centDigit ; i++){
      centScore = i * 100;
      sprintf(buf, "%03u", centScore);
      Serial.println(buf);
      drawString(buf, 3, 0, 0);
      ledMatrix.display();
      delay(150);
    }
    score = score - centScore;
  }

  if(score > 10){
    unsigned int dixDigit = (score / 10U) % 10;
    unsigned int dixScore = 0;
    char buf[2];
    for(int i = 1; i <= dixDigit ; i++){
      dixScore = i * 10;
      sprintf(buf, "%02u", dixScore);
      drawString(buf, 2, 0, 0);
      ledMatrix.display();
      delay(100);
    }
    score = score - dixScore;
  }

  if(score > 1){
    char buf[1];
    for(int i = 1; i <= score ; i++){
      sprintf(buf, "%01u", score);
      drawString(buf, 1, 0, 0);
      ledMatrix.display();
      delay(50);
    }
  }
}

void displayMessage(char* text, byte msgSpeed){
  int len = strlen(text);
  //for(int i = 31; i > len * -8; i--){
  for(int i = len * -8; i <= 31 ; i++){
    drawString(text, len, i, 0);
    ledMatrix.display();
    delay(msgSpeed);
  }
}


/**
 * This function draws a string of the given length to the given position.
 */
void drawString(char* text, int len, int x, int y )
{
  // reverse string : put the led matrix wrong side in the box...
  char reverse[len];
  byte car = 0;
  for(int i = len - 1; i >= 0; i--){
    reverse[car] = text[i];
    car++;
  }
  
  for( int idx = 0; idx < len; idx ++ )
  {
    int c = reverse[idx] - 32;

    // stop if char is outside visible area
    if( x + idx * 8  > 31 )
      return;

    // only draw if char is visible
    if( 8 + x + idx * 8 > 0 )
      drawSprite( font[c], x + idx * 8, y, 8, 8 );
  }
}

/**
 * This draws a sprite to the given position using the width and height supplied (usually 8x8)
 */
void drawSprite( byte* sprite, int x, int y, int width, int height )
{
  // The mask is used to get the column bit from the sprite row
  byte mask = B10000000;
  
  for( int iy = 0; iy < height; iy++ )
  {
    for( int ix = 0; ix < width; ix++ )
    {
      ledMatrix.setPixel(x + ix, y + iy, (bool)(sprite[iy] & mask ));

      // shift the mask by one pixel to the right
      mask = mask >> 1;
    }

    // reset column mask
    mask = B10000000;
  }
}

void clearSegment() {
  for (int i = 0; i < 8; i++) {
      segment.setDigit(i, LEDMatrixDriver::BCD_BLANK);
  }
  segment.display();
}

void showClacAnim(byte posAnim){
  byte  i = posAnim;
  //for(int i = 0; i < 33; i++){
    if(i == 0 || i == 6 || i == 12){
      drawSprite( CLAC[0], 16, 0, 8, 8 );
      drawSprite( CLAC[1], 8, 0, 8, 8 );
    } else if(i == 1 || i == 7 || i == 13){
      drawSprite( CLAC[2], 16, 0, 8, 8 );
       drawSprite( CLAC[3], 8, 0, 8, 8 );
    } else if(i == 2 || i == 8 || i == 14){
      drawSprite( CLAC[4], 16, 0, 8, 8 );
      drawSprite( CLAC[5], 8, 0, 8, 8 );
    } else if(i == 3 || i == 9 || i == 15){
      drawSprite( CLAC[6], 16, 0, 8, 8 );
      drawSprite( CLAC[7], 8, 0, 8, 8 ); 
    } else if(i == 4 || i == 10 || i == 16){
      drawSprite( CLAC[8], 16, 0, 8, 8 );
      drawSprite( CLAC[9], 8, 0, 8, 8 );
    }
    
    if(i > 4){
      byte moveClacL = 12 + i;
      int moveClacR = 12 -i;
      
      drawSprite( CLAC[8], moveClacL, 0, 8, 8 );
      drawSprite( CLAC[9], moveClacR, 0, 8, 8 );
      if(i > 10){
        drawSprite( CLAC[8], moveClacL - 6, 0, 8, 8 );
        drawSprite( CLAC[9], moveClacR + 6, 0, 8, 8 );

        if(i > 16){
          if(i > 25){
            drawSprite(font[35], 24, 0, 8, 8); //C
            drawSprite(font[35], 0, 0, 8, 8); //C
          }
          drawSprite(font[44], 16, 0, 8, 8); //L
          drawSprite(font[33], 8, 0, 8, 8); //A
          drawSprite( CLAC[8], moveClacL - 12, 0, 8, 8 );
          drawSprite( CLAC[9], moveClacR + 12, 0, 8, 8 );
        }
      }
      
    }
    ledMatrix.display();
}
