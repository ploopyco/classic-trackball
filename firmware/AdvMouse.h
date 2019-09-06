/*
  AdvMouse.h

  Copyright (c) 2015, Arduino LLC
  Original code (pre-library): Copyright (c) 2011, Peter Barrett

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ADVMOUSE_h
#define ADVMOUSE_h

#include "HID.h"

#if !defined(_USING_HID)

#warning "Using legacy HID core (non pluggable)"

#else

//================================================================================
//================================================================================
//  Mouse

#define MOUSE_LEFT    0x01
#define MOUSE_RIGHT   0x02
#define MOUSE_MIDDLE  0x04
#define MOUSE_BACK    0x08
#define MOUSE_FORWARD 0x10
#define MOUSE_ALL (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE | MOUSE_BACK | MOUSE_FORWARD)

class AdvMouse_
{
private:
  uint8_t _buttons;
  bool _isReportSent;
  void buttons(uint8_t b);
  void buttonsWithoutMove(uint8_t b);

public:
  AdvMouse_(void);
  void begin(void);
  void end(void);
  void click(uint8_t b = MOUSE_LEFT);
  void move(int16_t x, int16_t y, signed char wheel = 0); 
  void press(uint8_t b = MOUSE_LEFT);   // press LEFT by default
  void release(uint8_t b = MOUSE_LEFT); // release LEFT by default
  void press_(uint8_t b = MOUSE_LEFT);   // press LEFT by default, without Move sendreport
  void release_(uint8_t b = MOUSE_LEFT); // release LEFT by default, without Move sendreport
  bool needSendReport(void);
  bool isPressed(uint8_t b = MOUSE_LEFT); // check LEFT by default
};
extern AdvMouse_ AdvMouse;

#endif
#endif