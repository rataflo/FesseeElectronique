
#include "ADXL375.h"

int16_t read16(uint8_t reg) {
  Wire.beginTransmission(ADXL375_ADDRESS);
  i2cwrite(reg);
  Wire.endTransmission();
  Wire.requestFrom(ADXL375_ADDRESS, 2);
  return (uint16_t)(i2cread() | (i2cread() << 8));
}

/**************************************************************************/
/*!
    @brief  Gets the most recent X axis value
*/
/**************************************************************************/
int16_t getX(void) {
  return read16(ADXL375_DATAX0_REG);
}

/**************************************************************************/
/*!
    @brief  Gets the most recent Y axis value
*/
/**************************************************************************/
int16_t getY(void) {
  return read16(ADXL375_DATAY0_REG);
}

/**************************************************************************/
/*!
    @brief  Gets the most recent Z axis value
*/
/**************************************************************************/
int16_t getZ(void) {
  return read16(ADXL375_DATAZ0_REG);
}

inline void i2cwrite(uint8_t x) {
  #if ARDUINO >= 100
    Wire.write((uint8_t)x);
  #else
    Wire.send(x);
  #endif
}

inline uint8_t i2cread(void) {
  #if ARDUINO >= 100
    return Wire.read();
  #else
    return Wire.receive();
  #endif
}

void writeTo(int device, byte address, byte val) {
  Wire.beginTransmission(device); //start transmission to device 
  Wire.write(address);        // send register address
  Wire.write(val);        // send value to write
  Wire.endTransmission(); //end transmission
}

void readFrom(int device, byte address, int num, byte buff[]) {
  Wire.beginTransmission(device); //start transmission to device 
  Wire.write(address);        //sends address to read from
  Wire.endTransmission(); //end transmission

  Wire.beginTransmission(device); //start transmission to device (initiate again)
  Wire.requestFrom(device, num);    // request num bytes from device

  int i = 0;
  while(Wire.available())    //device may send less than requested (abnormal)
  { 
    buff[i] = Wire.read(); // receive a byte
    i++;
  }
  Wire.endTransmission(); //end transmission
}

void setupFIFO(){
  //   writeTo(DEVICE, 0x2D, 0);      
  //  writeTo(DEVICE, 0x2D, 16);
  //9.1 active low interrupt.  

  writeTo(DEVICE,ADXL375_DATA_FORMAT_REG,0x0B);

  //1. Write 0x28 to Register 0x1D; set shock threshold to 31.2 g.
  // writeTo(DEVICE,0x1D,0x28); // 2,5 Gs for an ADXL345,(OX28/0xFF)*FSR
  //writeTo(DEVICE,ADXL375_THRESH_TAP_REG,64); // 64/255 of fsr or 50 Gs since FSR == 200 Gs
  //2. Write 0x50 to Register 0x21; set shock duration to 50 ms.
  //writeTo(DEVICE,ADXL375_DUR_REG,0x50);
  //3. Write 0x20 to Register 0x22; set latency to 40 ms.
  //
  //writeTo(DEVICE,ADXL375_LATENT_REG,0x20);
  //4. Write 0xF0 to Register 0x23; set shock window to 300 ms.
  //writeTo(DEVICE,ADXL375_WINDOW_REG,0x1F);
  //5. Write 0x07 to Register 0x2A to enable X-, Y-, and Z-axes
  // participation in shock detection.
  //writeTo(DEVICE,ADXL375_TAP_AXES_REG,0x07);
  //6. Write 0x0F to Register 0x2C to set output data rate to
  //  3200 Hz.
  //writeTo(DEVICE,ADXL375_BW_RATE_REG,0X0F);
  //6. Write 0x01Dto Register 0x2C to set output data rate to
  //  800 Hz power save mode
//  writeTo(DEVICE,ADXL375_BW_RATE_REG,0X1D);
   //6. Write 0x01E to Register 0x2C to set output data rate to
  //  1600 Hz power save mode
  //writeTo(DEVICE,ADXL375_BW_RATE_REG,0X1E);
  //7. Write 0x40/0x20 to Register 0x2E to enable single shock or
  //  double shock, respectively.
  //writeTo(DEVICE,ADXL375_INT_ENABLE_REG,0x40);
  //8. Write 0x40/0x20 to Register 0x2F to assign single shock or
  //  double shock interrupt, respectively, to the INT2 pin.
 // writeTo(DEVICE,ADXL375_INT_MAP_REG,0X40);
  //9.  Write 0xEA to Register 0x38 to enable triggered mode
  //FIFO. If an interrupt is detected on the INT2 pin, the FIFO
  //records the trigger event acceleration with 10 samples
  //retained from before the trigger event.
  //writeTo(DEVICE,ADXL375_FIFO_CTL,0XEA);


  //10. Write 0x08 to Register 0x2D to start the measurement.
  //  It is recommended that the POWER_CTL register be
  //  configured last.

  // Thresh shock register*
  //writeTo(DEVICE,ADXL375_INT_MAP_REG, 0xBB); // All inteerupts on INT2 except single shock on INT1.
  //writeTo(DEVICE, ADXL375_INT_ENABLE_REG,0X40); // Only single shock interuuption activated
  //writeTo(DEVICE, ADXL375_FIFO_CTL,0XCA);//Trigger mode 11 + triger sur INT1 0 +  10 samples quand trigger 01010
  //uint8_t scaledValue = 5 * 1000 / 780;
  //writeTo(DEVICE, ADXL375_THRESH_TAP_REG,0x5); // 0 disable shock detection
  //writeTo(DEVICE, ADXL375_DUR_REG,0xFF);// Amount of time to register a shock. 0 disable the shock detection.
  
  //writeTo(DEVICE,ADXL375_LATENT_REG,0);// 0 disable the double shock detection.
  //writeTo(DEVICE,ADXL375_WINDOW_REG,0);// 0 disable the double shock detection.
  //writeTo(DEVICE,ADXL375_ACT_INACT_CTL_REG,0x0);// 0 disable activity/incativity test on all axis.
  //writeTo(DEVICE, ADXL375_TAP_AXES_REG,0x1);// Enable only z in shock detection
  //writeTo(DEVICE, ADXL375_BW_RATE_REG,ADXL375_800HZ);// 100 Hz speed 0X0A, 800Hz 0X1D max i2C
  writeTo(DEVICE, ADXL375_BW_RATE_REG, 0x0D);// 100 Hz speed 0X0A, 800Hz 0X1D max i2C
  writeTo(DEVICE,ADXL375_POWER_CTL_REG,0x08);
  
}
