/*
   La fessée électronique / The electronic spank.
   Spank machine like high striker/strong man game in funfair.
   Calculate velocity m/s, G impact and "clac" level of a spank.
   Score led strip animation inspired by bouncing balls effect => https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
   
   Florent Galès 2019 / Licence rien à branler / Do what the fuck you want licence.

   BOM:
     Arduino Nano.
     ADXL375 / GY-291 Accelerometer. Mix of diffenrent libraries like adafruit adxl345 (calculation) & https://github.com/georgeredinger/ADXL375_Testing/tree/master/ADXL375
     WS2812B led strip. Adafruit standard lib.
     Arcade switch button.
     8 digit 7 segment display. MAX7219 (SPI) : https://github.com/bartoszbielawski/LEDMatrixDriver
     4 x (8 x 8 led display) for score / high score. MAX7219.https://github.com/bartoszbielawski/LEDMatrixDriver
     Electret microphone
     Servo: use TiCoServo to work with neopixel. 
   
    PINS:
     ADXL 375 -> Arduino (I2C)
      GND (marron) -> GND
      VCC (orange) -> 3.3V
      SDA (vert) -> A4 / 4.75k resistor on 3.3v between A4 & SDA.
      SCL (bleu) -> A5 / 4.75k resistor on 3.3v between A5 & SCL
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

   Mic: A3 - 22kOhm between A3 and 5V.

   Servo for bell: D10 -> Use this pin or D9 for working with neopixel.
*/
#include "constants.h"
#include "ADXL375.h"
#include <LEDMatrixDriver.hpp>
#include <Adafruit_NeoPixel.h>
//#include <Adafruit_TiCoServo.h>
//#include <known_16bit_timers.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>
#include <MemoryUsage.h>


#define FASTADC 1
// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define MIN_Z 20 // Min value to store data in fifo.
#define MIN_CLAC 100 // Minimum for a spank.
#define DROP_CLAC 30 // Min drop after a max event.
#define SCORE_BELL 1500 // At which score we ring the bell. 1400
#define FIFOSIZE 30

#define PIN_MIC A3
#define PIN_BTN 4
#define PIN_SERVO_BELL 10
#define SERVO_MIN 1000 // 1 ms pulse
#define SERVO_MAX 2000 // 2 ms pulse

const char msgWorldRecord[] PROGMEM = {"WORLD RECORD"};
const char msgHighScore[] PROGMEM = {"HIGH SCORE"};
const char msgReady[] PROGMEM = {"READY"};
const char msgGo[] PROGMEM = {" GO!"};
const char *const msgTable[] PROGMEM = {msgWorldRecord, msgHighScore, msgReady, msgGo};
char bufferMsg[16]; 

#define PIN_STRIP_SCORE 6
#define NBLED_STRIP_SCORE 60
Adafruit_NeoPixel stripScore = Adafruit_NeoPixel(NBLED_STRIP_SCORE, PIN_STRIP_SCORE, NEO_GRB + NEO_KHZ800);

#define PIN_STRIP_CLAC 5
#define NBLED_STRIP_CLAC 10
Adafruit_NeoPixel stripClac = Adafruit_NeoPixel(NBLED_STRIP_CLAC, PIN_STRIP_CLAC, NEO_GRB + NEO_KHZ800);

LEDMatrixDriver segment (1, 8);
LEDMatrixDriver ledMatrix(4, 9); 

byte buff[6] ;    //6 bytes buffer for saving data read from the device
int fifo[FIFOSIZE];
bool bStartPlay = false;
int highScore = 0;
unsigned long lastClac = 0;
int maxZ = 0;
int maxMic = 0;

void setup(void)
{
  #if FASTADC
   // set prescale to 16
   sbi(ADCSRA,ADPS2) ;
   cbi(ADCSRA,ADPS1) ;
   cbi(ADCSRA,ADPS0) ;
  #endif

  Serial.begin(9600);

  // Servo bell
  pinMode(PIN_SERVO_BELL, OUTPUT);
  digitalWrite(PIN_SERVO_BELL, LOW);
  
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
  
  ledMatrix.setEnabled(true);
  ledMatrix.setIntensity(5); // 1= min, 15=max
  ledMatrix.clear();
  ledMatrix.display();
  
  // Adxl375 init
  Wire.begin();        // join i2c bus (address optional for master)
  readFrom(DEVICE, 0X30, 1, buff);
  // load high score from eeprom
  highScore = EEPROM.readInt(1);
  
  setupFIFO();
  clearFifo();
  resetDisplay();

  showScoreAnim(1501);
}

void loop(void)
{
  waitForStart();
  game();
  
}
  
void game(void)
{
  if(bStartPlay){

    unsigned long currentMillis = millis();

    // Calculation avoid use of float to speed up code.
    float x = getX() * ADXL375_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    float y = getY() * ADXL375_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    float z = getZ() * ADXL375_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD; //<= format en mG/LSB (milli G par Last Significant Bit). ADXL375 = 49mg/LSB. Donc z en G = z * 0.049
    int vector = sqrt((x * x) + (y * y) + (z * z));
    
    if(vector > MIN_Z && currentMillis > lastClac + 500){ // If last clac after 1/2 second.
      insertNewValue(vector);
      int mic = analogRead(A3);
      maxMic = mic > maxMic ? mic : maxMic;
      //Détection du maxiumum
      if(vector > maxZ){
        maxZ = vector;
      }else { // Max reached we test a big drop
       if(vector < maxZ - DROP_CLAC){
        if(maxZ > MIN_CLAC && checkValidity()) { //We got a real spank!
          mic = analogRead(A3);// Last read.
          
          maxMic = mic > maxMic ? mic : maxMic;
          Serial.println(maxMic);
          // Show fifo
          /*Serial.println("CLAC:");
          Serial.print("mic:");
          Serial.println(maxMic);
          for(byte i = 0; i < FIFOSIZE; i++){
            Serial.println(fifo[i]);
          }
          Serial.print("maxZ");Serial.println(maxZ);*/
          float gForce = maxZ / 9.8;
          int velocity = calcVelocity();
          //Serial.print("velocity=");Serial.println(velocity);
          //Serial.print("G=");Serial.println(gForce);
          float scoreGame = float(maxMic) + float(velocity) + gForce;
          //Serial.print("score=");Serial.println(scoreGame);
          clearFifo();
          lastClac = millis();

          showScoreAnim(scoreGame);
          displayMs(velocity);
          displayG(gForce);
          displayClacDb(maxMic);
          displayScore(round(scoreGame));
          maxZ = 0;
          maxMic = 0;
          // wait for 4s and reset.
          delay(4000);
          
          bStartPlay = false;
        } else { // false flag is not a "real" spank.
          clearFifo();
          maxZ = 0;
          maxMic = 0;
          lastClac = millis();
        }
       }
      }
    } else {
      clearFifo();
      maxZ = 0;
      maxMic = 0;
    }
  }
}


boolean checkValidity(){
  //The fifo must not include 0 values (case of a pinch on the paddle and not a swing movement).
  for(byte i = 0; i < FIFOSIZE; i++){
    if(fifo[i] == 0){
      return false;
    }
  }
  return true;
}

void insertNewValue(int z){
  for(byte i = FIFOSIZE - 1; i > 0; i--){
    fifo[i] = fifo[i - 1];
  }
  fifo[0] = z;
}

void clearFifo(){
  for(byte i = 0; i < FIFOSIZE; i++){
    fifo[i] = 0;
  }
}

int calcVelocity(){
  // calc average.
  int sumVelo = 0;
  int resultCalc = 0;

  for(byte i = 0; i < FIFOSIZE; i++){
    sumVelo += fifo[i];
  }
  resultCalc = sumVelo / FIFOSIZE;
  return resultCalc;
}


void showScoreAnim(float scoreGame) {
  bool bRingBell = false;
  float tmpScore = scoreGame;
  tmpScore = tmpScore > SCORE_BELL ? SCORE_BELL : tmpScore;
  byte level = map(tmpScore, 0, SCORE_BELL, 0, NBLED_STRIP_SCORE);
  byte posAnim = 0;
  unsigned long lastAnimMillis = 0;
  float hTemp ;     
  float hauteur = tmpScore / SCORE_BELL;// An array of heights
  float vImpact = sqrt( -2 * -1 * hauteur ); ;                   // As time goes on the impact velocity will change, so make an array to store those values
  float tCycle ;                    // The time since the last time the ball struck the ground
  int   posBall ;                       // The integer position of the dot on the strip (LED index)
  long  tLast ;                     // The clock time of the last ground strike
  tLast = millis() - 1;
  hTemp = hauteur;
  posBall = 0;
  tCycle = 0;

  while (hTemp > 0) {
    unsigned long currentMillis = millis();
    tCycle =  currentMillis - tLast ;     // Calculate the time since the last time the ball was on the ground
    // Calculate position with time, acceleration (gravity) and intial velocity
    hTemp = 0.5 * -0.98 * pow( tCycle/1000 , 2.0 ) + vImpact * tCycle/1000;
    posBall = round( hTemp * (level - 1) / hauteur);       // Map hauteur to pixel pos
    stripScore.setPixelColor(posBall, 255, 0, 0);
    stripScore.show();
    //Off for next loop.
    stripScore.setPixelColor(posBall, 0, 0, 0);

    Serial.println(bRingBell);
    Serial.println(posBall);
    Serial.println(scoreGame);
    if(!bRingBell && posBall == NBLED_STRIP_SCORE && scoreGame > SCORE_BELL){
      Serial.println("ding");
      ringBell();
      bRingBell = true;
    }
    
    if(currentMillis - lastAnimMillis > 30 && posAnim < 33){
      lastAnimMillis = currentMillis;
      showClacAnim(posAnim);
      posAnim++;
    }
 }

  // Finish matrix animation.
  while(posAnim < 33){
    unsigned long currentMillis = millis();
    if(currentMillis - lastAnimMillis > 30){
      lastAnimMillis = currentMillis;
      showClacAnim(posAnim);
      posAnim++;
    }
  }
}

void ringBell(){
  digitalWrite(PIN_SERVO_BELL, HIGH);
  delay(500);
  digitalWrite(PIN_SERVO_BELL, LOW);
}

void displayClacDb(int mic) {
  byte level = map(mic, 0, 1023, 0, 10);

  for(byte i = 9; i >= 10 - level; i --){
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

void displayMs(int ms) {
  ms = ms > 999 ? 999 : ms;
  char buf[4];
  sprintf(buf, "%04u", ms);
  segment.setDigit(7, ms >= 100 ? buf[1] : LEDMatrixDriver::BCD_BLANK, false);
  segment.setDigit(6, ms >= 10 ? buf[2] : LEDMatrixDriver::BCD_BLANK, false);
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

void displayScore(int scoreGame)
{
  bool bHighScore = false;
  if(scoreGame > 9999){
    scoreGame = 9999;
  }
  if(scoreGame > highScore){
    highScore = scoreGame;
    bHighScore = true;
  }
  ledMatrix.clear();
  ledMatrix.display();

  if(scoreGame > 1000){
    int milleDigit = (scoreGame / 1000U) % 10;
    int milleScore = 0;
    char bufMille[5];
    for(int i = 1; i <= milleDigit ; i++){
      milleScore = i * 1000;
      sprintf(bufMille, "%04u", milleScore);
      drawString(bufMille, 4, 0, 0);
      ledMatrix.display();
      delay(300);
    }
    scoreGame = scoreGame - milleScore;
  }

  if(scoreGame > 100){
    int centDigit = (scoreGame / 100U) % 10;
    int centScore = 0;
    char bufCent[4];
    for(int i = 1; i <= centDigit ; i++){
      centScore = i * 100;
      sprintf(bufCent, "%03u", centScore);
      drawString(bufCent, 3, 0, 0);
      ledMatrix.display();
      delay(150);
    }
    scoreGame = scoreGame - centScore;
  }

  if(scoreGame > 10){
    int dixDigit = (scoreGame / 10U) % 10;
    int dixScore = 0;
    char bufDix[3];
    for(int i = 1; i <= dixDigit ; i++){
      dixScore = i * 10;
      sprintf(bufDix, "%02u", dixScore);
      drawString(bufDix, 2, 0, 0);
      ledMatrix.display();
      delay(100);
    }
    scoreGame = scoreGame - dixScore;
  }

  if(scoreGame > 1){
    char bufDigit[2];
    for(int i = 1; i <= scoreGame ; i++){
      sprintf(bufDigit, "%01u", scoreGame);
      drawString(bufDigit, 1, 0, 0);
      ledMatrix.display();
      delay(50);
    }
  }

  if(bHighScore){
    delay(1000);
    strcpy_P(bufferMsg, (char *)pgm_read_word(&(msgTable[0])));
    displayMessage(bufferMsg, 20);// world record message
    // save in eeprom.
    EEPROM.writeInt(1, highScore);
  }
}

void displayMessage(char* text, byte msgSpeed){
  byte len = strlen(text);
  //for(int i = 31; i > len * -8; i--){
  for(int i = len * -8; i <= 31 ; i++){
    if(digitalRead(PIN_BTN) == HIGH){
      drawString(text, len, i, 0);
      ledMatrix.display();
      delay(msgSpeed);
    } else {
      return;
    }
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
  byte image[8];
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
    if( 8 + x + idx * 8 > 0 ){
      memcpy_P(&image, &font[c], 8);
      drawSprite(image, x + idx * 8, y, 8, 8 );
    }
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
  
  byte image[8];    
  byte  i = posAnim;
  //for(int i = 0; i < 33; i++){
    if(i == 0 || i == 6 || i == 12){
      memcpy_P(&image, &CLAC[0], 8);
      drawSprite(image, 16, 0, 8, 8 );
      memcpy_P(&image, &CLAC[1], 8);
      drawSprite(image, 16, 0, 8, 8 );
    } else if(i == 1 || i == 7 || i == 13){
      memcpy_P(&image, &CLAC[2], 8);
      drawSprite(image, 16, 0, 8, 8 );
      memcpy_P(&image, &CLAC[3], 8);
      drawSprite(image, 8, 0, 8, 8 );
    } else if(i == 2 || i == 8 || i == 14){
      memcpy_P(&image, &CLAC[4], 8);
      drawSprite(image, 16, 0, 8, 8 );
      memcpy_P(&image, &CLAC[5], 8);
      drawSprite(image, 8, 0, 8, 8 );
    } else if(i == 3 || i == 9 || i == 15){
      memcpy_P(&image, &CLAC[61], 8);
      drawSprite(image, 16, 0, 8, 8 );
      memcpy_P(&image, &CLAC[7], 8);
      drawSprite(image, 8, 0, 8, 8 ); 
    } else if(i == 4 || i == 10 || i == 16){
      memcpy_P(&image, &CLAC[8], 8);
      drawSprite(image, 16, 0, 8, 8 );
      memcpy_P(&image, &CLAC[9], 8);
      drawSprite(image, 8, 0, 8, 8 );
    }
    
    if(i > 4){
      byte moveClacL = 12 + i;
      int moveClacR = 12 -i;
      memcpy_P(&image, &CLAC[8], 8);
      drawSprite(image, moveClacL, 0, 8, 8 );
      memcpy_P(&image, &CLAC[9], 8);
      drawSprite(image, moveClacR, 0, 8, 8 );
      if(i > 10){
        memcpy_P(&image, &CLAC[8], 8);
        drawSprite(image, moveClacL - 6, 0, 8, 8 );
        memcpy_P(&image, &CLAC[9], 8);
        drawSprite(image, moveClacR + 6, 0, 8, 8 );

        if(i > 16){
          if(i > 25){
            memcpy_P(&image, &font[35], 8);
            drawSprite(image, 24, 0, 8, 8); //C
            memcpy_P(&image, &font[35], 8);
            drawSprite(image, 0, 0, 8, 8); //C
          }
          memcpy_P(&image, &font[44], 8);
          drawSprite(image, 16, 0, 8, 8); //L
          memcpy_P(&image, &font[33], 8);
          drawSprite(image, 8, 0, 8, 8); //A
          memcpy_P(&image, &CLAC[8], 8);
          drawSprite(image, moveClacL - 12, 0, 8, 8 );
          memcpy_P(&image, &CLAC[19], 8);
          drawSprite(image, moveClacR + 12, 0, 8, 8 );
        }
      }
      
    }
    ledMatrix.display();
}

void resetDisplay(){
  ledMatrix.clear();
  ledMatrix.display();
  clearSegment();
  stripScore.clear();
  stripScore.setPixelColor(0, 255, 0, 0);
  stripScore.show();
  for(byte i = 0; i < 10; i++){
    stripClac.setPixelColor(i, 0, 0, 0);
  }
  stripClac.show(); 
}



void waitForStart(){
  
  if(!bStartPlay){
    unsigned long delayShowScore = 0;
    unsigned long currentMillis = millis();
    resetDisplay();
  
    while(!bStartPlay){
      while(digitalRead(PIN_BTN) == HIGH){
        currentMillis = millis();
        // Erase previous game after 5 sec delay.
        if(currentMillis > delayShowScore){
          delayShowScore = currentMillis + 10000;
          resetDisplay();
          strcpy_P(bufferMsg, (char *)pgm_read_word(&(msgTable[1])));
          displayMessage(bufferMsg, 20);// high score message
          if(digitalRead(PIN_BTN) == HIGH){
            displayScore(highScore);
          }
        }
      }
      delay(300);
      bStartPlay = true;
      resetDisplay();
      strcpy_P(bufferMsg, (char *)pgm_read_word(&(msgTable[2])));
      displayMessage(bufferMsg, 20);// ready message.
      strcpy_P(bufferMsg, (char *)pgm_read_word(&(msgTable[3])));
      drawString(bufferMsg, 4, 0, 0); // Go! message.
      ledMatrix.display();
    }
  }
}
