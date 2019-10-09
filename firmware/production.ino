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
#include <avr/wdt.h>
#include "AdvMouse.h"

// Max recommended is 4,000,000, since clock speed is 8MHz.
// If lower, it should be by factors of 1/2.
static const int SPIMAXIMUMSPEED = 2000000;

static const int CPI = 1200;
static const int DEBOUNCE = 10; // ms
static const int SCROLL_DEBOUNCE = 50; // ms
static const int SCROLL_BUTT_DEBOUNCE = 100; // ms

static const float ROTATIONAL_TRANSFORM_ANGLE = 20;

static const int MOUSE_LEFT_PIN = 4;
static const int MOUSE_RIGHT_PIN = 7;
static const int MOUSE_MIDDLE_PIN = 0;
static const int MOUSE_BACK_PIN = 9;
static const int MOUSE_FORWARD_PIN = 6;

// For info, see https://forum.arduino.cc/index.php?topic=241369.0
static const int SENSOR_CS = 17;

static const int OPT_ENC_PIN1 = A3;
static const int OPT_ENC_PIN2 = A5;

static const int OPT_LOW_THRESHOLD = 150;

static const byte MOTION = 0x02;
static const byte DELTA_X_L = 0x03;
static const byte DELTA_X_H = 0x04;
static const byte DELTA_Y_L = 0x05;
static const byte DELTA_Y_H = 0x06;
static const byte CONFIG1 = 0x0F;
static const byte CONFIG2 = 0x10;
static const byte SROM_ENABLE = 0x13;
static const byte SROM_ID = 0x2A;
static const byte POWER_UP_RESET = 0x3A;
static const byte SHUTDOWN = 0x3B;
static const byte MOTION_BURST = 0x50;
static const byte SROM_LOAD_BURST = 0x62;

boolean debugMode = false;
boolean wdtMode = false;

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

unsigned long lastScroll = 0;

unsigned long middleClickRelease = 0;

extern const unsigned short firmware_length;
extern const unsigned char firmware_data[];

void setup() {
  if (wdtMode) {
    setupWDT();
  }

  if (debugMode) {
    Serial.begin(9600);
  }

  // Startup delay. This sometimes prevents a USB handshake lockup.
  delay(1000);

  setupPins();

  // This is the debug LED.
  pinMode(A0, OUTPUT);
  if (debugMode) {
    digitalWrite(A0, HIGH);
  } else {
    digitalWrite(A0, LOW);
  }

  SPI.begin();
  SPI.setDataMode(SPI_MODE3);
  SPI.setBitOrder(MSBFIRST);

  initialisePMW3360();

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

void setupWDT() {
  /* IMPORTANT NOTE!
   * THIS MUST BE THE FIRST THING THAT RUNS IN SETUP()! If it isn't, there's
   * a good chance the Arduino will enter an unstable state, and it's entirely
   * possible that you can permanently brick your MCU!
   */

  cli();

  // Clear the watchdog reset flag
  MCUSR &= ~(1 << WDRF);

  // Enter watchdog configuration mode
  WDTCSR |= (1 << WDCE) |
            (1 << WDE);

  // WDIE needs to be included, or else this doesn't work.
  WDTCSR = (1 << WDIE) |
           (1 << WDE) |
           (1 << WDP3) |
           (0 << WDP2) |
           (0 << WDP1) |
           (0 << WDP0);

  sei();
}

void setupPins() {
  pinMode(SENSOR_CS, OUTPUT);
  pinMode(MOUSE_LEFT_PIN, INPUT_PULLUP);
  pinMode(MOUSE_RIGHT_PIN, INPUT_PULLUP);
  pinMode(MOUSE_MIDDLE_PIN, INPUT_PULLUP);
  pinMode(MOUSE_BACK_PIN, INPUT_PULLUP);
  pinMode(MOUSE_FORWARD_PIN, INPUT_PULLUP);

  pinMode(OPT_ENC_PIN1, INPUT);
  pinMode(OPT_ENC_PIN2, INPUT);

  /* Ground all output pins connected to ground. This provides additional
   * pathways to ground. If you're messing with this, know this: driving ANY
   * of these pins high will cause a short. On the MCU. Ka-blooey.
   */
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
}

void adnsComBegin() {
  digitalWrite(SENSOR_CS, LOW);
}

void adnsComEnd() {
  digitalWrite(SENSOR_CS, HIGH);
}

byte adnsReadReg(byte reg_addr) {
  adnsComBegin();

  // send adress of the register, with MSBit = 0 to indicate it's a read
  SPI.transfer(reg_addr & 0x7f);
  delayMicroseconds(35); // tSRAD
  byte data = SPI.transfer(0);

  // tSCLK-SENSOR_CS for read operation is 120ns
  delayMicroseconds(1);
  adnsComEnd();
  // tSRW/tSRR (=20us) minus tSCLK-SENSOR_CS
  delayMicroseconds(19);

  return data;
}

void adnsWriteReg(byte reg_addr, byte data) {
  adnsComBegin();

  // send adress of the register, with MSBit = 1 to indicate it's a write
  SPI.transfer(reg_addr | 0x80);
  SPI.transfer(data);

  // tSCLK-SENSOR_CS for write operation
  delayMicroseconds(20);
  adnsComEnd();
  // tSWW/tSWR (=120us) minus tSCLK-SENSOR_CS.
  // Could be shortened, but this looks like a safe lower-bound.
  delayMicroseconds(100);
}

void setCPI(int cpi) {
  int cpival = constrain((cpi / 100) - 1, 0, 0x77); // limits to 0--119

  adnsComBegin();
  adnsWriteReg(CONFIG1, cpival);
  adnsComEnd();

  if (debugMode) {
    Serial.print(F("Got "));
    Serial.println(cpi);
    Serial.print(F("Set cpi to "));
    Serial.println((cpival + 1) * 100);
  }
}

void initialisePMW3360(void) {
  if (debugMode) {
    Serial.println(F("Initialising PMW3360"));
  }

  // Hard reset. Start by ensuring that the serial port is reset.
  adnsComEnd();
  adnsComBegin();
  adnsComEnd();

  // SHUTDOWN the PMW3360, in case the microcontroller reset but the sensor didn't.
  adnsWriteReg(SHUTDOWN, 0xb6);
  delay(300);

  // Drop and raise SENSOR_CS to reset the spi port
  adnsComBegin();
  delayMicroseconds(40);
  adnsComEnd();
  delayMicroseconds(40);

  // Force a reset of the PMW3360.
  adnsWriteReg(POWER_UP_RESET, 0x5a);
  delay(50);

  // Read registers 0x02 through 0x06 (and discard the data)
  adnsReadReg(MOTION);
  adnsReadReg(DELTA_X_L);
  adnsReadReg(DELTA_X_H);
  adnsReadReg(DELTA_Y_L);
  adnsReadReg(DELTA_Y_H);

  if (debugMode) {
    Serial.println(F("Uploading firmware to PMW3360"));
  }

  // Write 0 to Rest_En bit of CONFIG2 register to disable Rest mode.
  adnsWriteReg(CONFIG2, 0x00);

  // write 0x1d in SROM_enable reg for initializing
  adnsWriteReg(SROM_ENABLE, 0x1d);

  // wait for more than one frame period
  // assume that the frame rate is as low as 100fps, even if it should never be that low
  delay(10);

  // write 0x18 to SROM_enable to start SROM download
  adnsWriteReg(SROM_ENABLE, 0x18);

  // write burst destination adress
  adnsComBegin();
  SPI.transfer(SROM_LOAD_BURST | 0x80);
  delayMicroseconds(15);

  unsigned char c;
  for (int i = 0; i < firmware_length; i++) {
    c = (unsigned char) pgm_read_byte(firmware_data + i);
    SPI.transfer(c);
    delayMicroseconds(15);
  }

  // Read the SROM_ID register to verify the ID before any other register reads or writes.
  adnsReadReg(SROM_ID);

  // Write 0x00 (rest disable) to CONFIG2 register for wired mouse.
  // (If this were a wireless design, it'd be 0x20.)
  adnsWriteReg(CONFIG2, 0x00);

  adnsComEnd();

  if (debugMode) {
    Serial.println(F("Firmware successfully written to PMW3360"));
  }

  delay(10);

  setCPI(CPI);

  // start burst mode
  adnsWriteReg(MOTION_BURST, 0x00);

  if (debugMode) {
    Serial.println(F("PMW3360 successfully initialized"));
  }
}


void checkButtonState() {
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

      if (buttonPin[i] == MOUSE_MIDDLE_PIN) {
        middleClickRelease = micros();
      }
    }
  }
}

signed char moveWheel() {
  // If the mouse wheel was just released, do not scroll.
  unsigned long elapsed = micros() - middleClickRelease;
  if (elapsed < SCROLL_BUTT_DEBOUNCE) {
    return 0;
  }

  // Limit the number of scrolls per unit time.
  elapsed = micros() - lastScroll;
  if (elapsed < SCROLL_DEBOUNCE) {
    return 0;
  }

  // Don't scroll if the middle button is depressed.
  int middleButtonPin = digitalRead(MOUSE_MIDDLE_PIN);
  if (middleButtonPin == LOW) {
    return 0;
  }
  
  lastScroll = micros();
  
  int d1 = analogRead(OPT_ENC_PIN1);
  int d2 = analogRead(OPT_ENC_PIN2);

  if (debugMode) {
    Serial.print(F("d1: "));
    Serial.print(d1);
    Serial.print(F(", d2: "));
    Serial.println(d2);
  }

  signed char ret = 0;

  if (d1 < OPT_LOW_THRESHOLD && d2 < OPT_LOW_THRESHOLD) {
    if (firstOptLowPin == OPT_ENC_PIN1) {
      // scroll down
      ret = -1;
    } else if (firstOptLowPin == OPT_ENC_PIN2) {
      // scroll up
      ret = 1;
    }

    firstOptLowPin = 0;
  } else if (d1 < OPT_LOW_THRESHOLD) {
    firstOptLowPin = OPT_ENC_PIN1;
  } else if (d2 < OPT_LOW_THRESHOLD) {
    firstOptLowPin = OPT_ENC_PIN2;
  }

  return ret;
}

void loop() {
  if (wdtMode) {
    wdt_reset();
  }

  byte burstBuffer[12];
  unsigned long elapsed = micros() - lastTS;

  checkButtonState();

  // polling interval : more than > 0.5 ms.
  if (elapsed > 870) { // 870...whut?
    adnsComBegin();
    SPI.beginTransaction(SPISettings(SPIMAXIMUMSPEED, MSBFIRST, SPI_MODE3));

    SPI.transfer(MOTION_BURST);
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

    adnsComEnd();

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
