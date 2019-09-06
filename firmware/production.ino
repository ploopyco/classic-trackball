/*
 * Copyright (c) 2019, Ploopy Corporation, a Canadian federal corporation.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <SPI.h>
#include <avr/pgmspace.h>
#include "AdvMouse.h"

boolean debugMode = false;

const static int CPI = 1200;
const static int DEBOUNCE = 10; // ms

const static float ROTATIONAL_TRANSFORM_ANGLE = 27.5;

const static int MOUSE_LEFT_PIN = 4;
const static int MOUSE_RIGHT_PIN = 7;
const static int MOUSE_MIDDLE_PIN = 0;
const static int MOUSE_BACK_PIN = 9;
const static int MOUSE_FORWARD_PIN = 6;

const static int OPT_ENC_PIN1 = A3;
const static int OPT_ENC_PIN2 = A5;

const static int OPT_LOW_THRESHOLD = 100;

const static byte Product_ID = 0x00;
const static byte Revision_ID = 0x01;
const static byte Motion = 0x02;
const static byte Delta_X_L = 0x03;
const static byte Delta_X_H = 0x04;
const static byte Delta_Y_L = 0x05;
const static byte Delta_Y_H = 0x06;
const static byte SQUAL = 0x07;
const static byte Raw_Data_Sum = 0x08;
const static byte Maximum_Raw_data = 0x09;
const static byte Minimum_Raw_data = 0x0A;
const static byte Shutter_Lower = 0x0B;
const static byte Shutter_Upper = 0x0C;
const static byte Control = 0x0D;
const static byte Config1 = 0x0F;
const static byte Config2 = 0x10;
const static byte Angle_Tune = 0x11;
const static byte Frame_Capture = 0x12;
const static byte SROM_Enable = 0x13;
const static byte Run_Downshift = 0x14;
const static byte Rest1_Rate_Lower = 0x15;
const static byte Rest1_Rate_Upper = 0x16;
const static byte Rest1_Downshift = 0x17;
const static byte Rest2_Rate_Lower = 0x18;
const static byte Rest2_Rate_Upper = 0x19;
const static byte Rest2_Downshift = 0x1A;
const static byte Rest3_Rate_Lower = 0x1B;
const static byte Rest3_Rate_Upper = 0x1C;
const static byte Observation = 0x24;
const static byte Data_Out_Lower = 0x25;
const static byte Data_Out_Upper = 0x26;
const static byte Raw_Data_Dump = 0x29;
const static byte SROM_ID = 0x2A;
const static byte Min_SQ_Run = 0x2B;
const static byte Raw_Data_Threshold = 0x2C;
const static byte Config5 = 0x2F;
const static byte Power_Up_Reset = 0x3A;
const static byte Shutdown = 0x3B;
const static byte Inverse_Product_ID = 0x3F;
const static byte LiftCutoff_Tune3 = 0x41;
const static byte Angle_Snap = 0x42;
const static byte LiftCutoff_Tune1 = 0x4A;
const static byte Motion_Burst = 0x50;
const static byte LiftCutoff_Tune_Timeout = 0x58;
const static byte LiftCutoff_Tune_Min_Length = 0x5A;
const static byte SROM_Load_Burst = 0x62;
const static byte Lift_Config = 0x63;
const static byte Raw_Data_Burst = 0x64;
const static byte LiftCutoff_Tune2 = 0x65;

// Pin 17? WHAA? Curious? See here:
// https://forum.arduino.cc/index.php?topic=241369.0
const static int SENSOR_CS = 17;

float cosTransform;
float sinTransform;

int buttonPin[5] = { MOUSE_LEFT_PIN, MOUSE_RIGHT_PIN, MOUSE_MIDDLE_PIN, MOUSE_BACK_PIN, MOUSE_FORWARD_PIN };
bool buttonState[5] = { false, false, false, false, false };
uint8_t buttonBuffer[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
char buttonKey[5] = { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_BACK, MOUSE_FORWARD };

int firstOptLowPin;

bool initComplete = false;

int16_t dx;
int16_t dy;

unsigned long lastTS;
unsigned long lastButtonCheck = 0;

extern const unsigned short firmware_length;
extern const unsigned char firmware_data[];

void setup() {
  if (debugMode) {
    pinMode(A0, OUTPUT);
    digitalWrite(A0, HIGH);
    //delay(5000);
  }
  
  // Startup delay. This sometimes prevents a USB handshake lockup.
  delay(1000);

  Serial.begin(9600);

  pinMode(SENSOR_CS, OUTPUT);
  pinMode(MOUSE_LEFT_PIN, INPUT_PULLUP);
  pinMode(MOUSE_RIGHT_PIN, INPUT_PULLUP);
  pinMode(MOUSE_MIDDLE_PIN, INPUT_PULLUP);
  pinMode(MOUSE_BACK_PIN, INPUT_PULLUP);
  pinMode(MOUSE_FORWARD_PIN, INPUT_PULLUP);

  pinMode(OPT_ENC_PIN1, INPUT);
  pinMode(OPT_ENC_PIN2, INPUT);

  // Ground all output pins connected to ground.
  // This provides additional pathways to ground.
  // If you're messing with this, know this:
  // Driving ANY of these pins high will cause a short.
  // On the MCU.
  // Ka-blooey.
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A4, OUTPUT);
  digitalWrite(1, LOW);
  digitalWrite(2, LOW);
  digitalWrite(8, LOW);
  digitalWrite(10, LOW);
  digitalWrite(11, LOW);
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);
  digitalWrite(A1, LOW);
  digitalWrite(A2, LOW);
  digitalWrite(A4, LOW);

  SPI.begin();
  SPI.setDataMode(SPI_MODE3);
  SPI.setBitOrder(MSBFIRST);

  performStartup();

  cosTransform = cos(ROTATIONAL_TRANSFORM_ANGLE * PI / 180);
  sinTransform = sin(ROTATIONAL_TRANSFORM_ANGLE * PI / 180);

  dx = 0;
  dy = 0;

  delay(1000);

  initComplete = true;

  lastTS = micros();

  AdvMouse.begin();

  if (debugMode) {
    Serial.println(F("end setup"));
  }
}

void adns_com_begin() {
  digitalWrite(SENSOR_CS, LOW);
}

void adns_com_end() {
  digitalWrite(SENSOR_CS, HIGH);
}

byte adns_read_reg(byte reg_addr) {
  adns_com_begin();

  // send adress of the register, with MSBit = 0 to indicate it's a read
  SPI.transfer(reg_addr & 0x7f);
  delayMicroseconds(35); // tSRAD
  byte data = SPI.transfer(0);

  // tSCLK-SENSOR_CS for read operation is 120ns
  delayMicroseconds(1);
  adns_com_end();
  // tSRW/tSRR (=20us) minus tSCLK-SENSOR_CS
  delayMicroseconds(19);

  return data;
}

void adns_write_reg(byte reg_addr, byte data) {
  adns_com_begin();

  // send adress of the register, with MSBit = 1 to indicate it's a write
  SPI.transfer(reg_addr | 0x80);
  SPI.transfer(data);

  // tSCLK-SENSOR_CS for write operation
  delayMicroseconds(20);
  adns_com_end();
  // tSWW/tSWR (=120us) minus tSCLK-SENSOR_CS.
  // Could be shortened, but this looks like a safe lower-bound.
  delayMicroseconds(100);
}

void adns_upload_firmware() {
  if (debugMode) {
    Serial.println(F("Uploading firmware to PMW3360"));
  }

  // Write 0 to Rest_En bit of Config2 register to disable Rest mode.
  adns_write_reg(Config2, 0x00);

  // write 0x1d in SROM_enable reg for initializing
  adns_write_reg(SROM_Enable, 0x1d);

  // wait for more than one frame period
  // assume that the frame rate is as low as 100fps, even if it should never be that low
  delay(10);

  // write 0x18 to SROM_enable to start SROM download
  adns_write_reg(SROM_Enable, 0x18);

  // write burst destination adress
  adns_com_begin();
  SPI.transfer(SROM_Load_Burst | 0x80);
  delayMicroseconds(15);

  unsigned char c;
  for (int i = 0; i < firmware_length; i++) {
    c = (unsigned char) pgm_read_byte(firmware_data + i);
    SPI.transfer(c);
    delayMicroseconds(15);
  }

  // Read the SROM_ID register to verify the ID before any other register reads or writes.
  adns_read_reg(SROM_ID);

  // Write 0x00 (rest disable) to Config2 register for wired mouse.
  // (If this were a wireless design, it'd be 0x20.)
  adns_write_reg(Config2, 0x00);

  adns_com_end();

  if (debugMode) {
    Serial.println(F("Firmware successfully written to PMW3360"));
  }
}

void setCPI(int cpi) {
  int cpival = constrain((cpi / 100) - 1, 0, 0x77); // limits to 0--119

  adns_com_begin();
  adns_write_reg(Config1, cpival);
  adns_com_end();

  if (debugMode) {
    Serial.print(F("Got "));
    Serial.println(cpi);
    Serial.print(F("Set cpi to "));
    Serial.println((cpival + 1) * 100);
  }
}

void performStartup(void) {
  // Hard reset. Start by ensuring that the serial port is reset.
  adns_com_end();
  adns_com_begin();
  adns_com_end();

  // Shutdown the PMW3360, in case the microcontroller reset but the sensor didn't.
  adns_write_reg(Shutdown, 0xb6);
  delay(300);

  // Drop and raise SENSOR_CS to reset the spi port
  adns_com_begin();
  delayMicroseconds(40);
  adns_com_end();
  delayMicroseconds(40);

  // Force a reset of the PMW3360.
  adns_write_reg(Power_Up_Reset, 0x5a);
  delay(50);

  // Read registers 0x02 through 0x06 (and discard the data)
  adns_read_reg(Motion);
  adns_read_reg(Delta_X_L);
  adns_read_reg(Delta_X_H);
  adns_read_reg(Delta_Y_L);
  adns_read_reg(Delta_Y_H);

  adns_upload_firmware();
  delay(10);

  setCPI(CPI);

  // start burst mode
  adns_write_reg(Motion_Burst, 0x00);

  if (debugMode) {
    Serial.println(F("PMW3360 successfully initialized"));
  }
}


void check_button_state() {
  if (!initComplete)
    return;

  unsigned long elapsed = micros() - lastButtonCheck;

  // Update at a period of 1/8 of the DEBOUNCE time
  if (elapsed < (DEBOUNCE * 1000UL / 8)) {
    return;
  }

  lastButtonCheck = micros();

  // Fast debounce (works with 0 latency most of the time)
  for (int i = 0; i < 5; i++) {
    int curButtonState = digitalRead(buttonPin[i]);
    buttonBuffer[i] = buttonBuffer[i] << 1 | curButtonState;

    if (!buttonState[i] && buttonBuffer[i] == 0xFE) {
      // First time the button press is being registered.
      AdvMouse.press_(buttonKey[i]);
      buttonState[i] = true;
    } else if ((buttonState[i] && buttonBuffer[i] == 0x01) ||
               (buttonState[i] && buttonBuffer[i] == 0xFF)) {
      // Either the button is being released, or the debounce time elapsed.
      AdvMouse.release_(buttonKey[i]);
      buttonState[i] = false;
    }
  }
}

void loop() {
  byte burstBuffer[12];
  unsigned long elapsed = micros() - lastTS;

  check_button_state();

  // polling interval : more than > 0.5 ms.
  if (elapsed > 870) { // 870...whut?
    adns_com_begin();
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE3));

    SPI.transfer(Motion_Burst);
    // Wait for tSRAD
    delayMicroseconds(35);

    // read burst buffer
    SPI.transfer(burstBuffer, 12);
    // tSCLK-SENSOR_CS for read operation is 120ns
    delayMicroseconds(1);

    SPI.endTransaction();

    int motion = (burstBuffer[0] & 0x80) > 0;

    int xl = burstBuffer[2];
    int xh = burstBuffer[3];
    int yl = burstBuffer[4];
    int yh = burstBuffer[5];

    int x = xh << 8 | xl;
    int y = yh << 8 | yl;

    // Rotate the x and y results, since the sensor isn't quite squared up in
    // the enclosure.
    // See here: // See here: https://en.wikipedia.org/wiki/Rotation_matrix
    int xPrime = round(((float) x * cosTransform) - ((float) y * sinTransform));
    int yPrime = round(((float) x * sinTransform) + ((float) y * cosTransform));

    dx -= xPrime;
    dy += yPrime;

    adns_com_end();

    signed char wheel = moveWheel();

    // update only if a movement is detected.
    if (AdvMouse.needSendReport() || motion || wheel != 0) {
      AdvMouse.move(dx, dy, wheel);
      dx = 0;
      dy = 0;
    }

    lastTS = micros();
  }
}

signed char moveWheel() {
  int d1 = analogRead(OPT_ENC_PIN1);
  int d2 = analogRead(OPT_ENC_PIN2);

  signed char ret = 0;

  if (d1 < OPT_LOW_THRESHOLD && d2 < OPT_LOW_THRESHOLD) {
    if (firstOptLowPin == OPT_ENC_PIN1) {
      // scroll up
      ret = 1;
    } else if (firstOptLowPin == OPT_ENC_PIN2) {
      // scroll down
      ret = -1;
    }

    firstOptLowPin = 0;
  } else if (d1 < OPT_LOW_THRESHOLD) {
    firstOptLowPin = OPT_ENC_PIN1;
  } else if (d2 < OPT_LOW_THRESHOLD) {
    firstOptLowPin = OPT_ENC_PIN2;
  }

  return ret;
}
