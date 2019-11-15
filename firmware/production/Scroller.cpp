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
 * You should have received a copy of the GNU General Public Licensefalse
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Scroller.h"

Scroller::Scroller(void) {
  state = HIHI;
  lohif = false;
  hilof = false;
  lowA = 1023;
  highA = 0;
  lowB = 1023;
  highB = 0;
  scrollThresholdA = 0;
  scrollThresholdB = 0;
}

int Scroller::scroll(int curA, int curB) {
  calculateThresholdA(curA);
  calculateThresholdB(curB);

  bool LO = false;
  bool HI = true;
  bool sA, sB;
  int ret = 0;

  if (curA < scrollThresholdA)
    sA = LO;
  else
    sA = HI;

  if (curB < scrollThresholdB)
    sB = LO;
  else
    sB = HI;

  if (state == HIHI) {
    if (sA == LO && sB == HI) {
      state = LOHI;
      if (hilof) {
        ret = 1;
        hilof = false;
      }
    } else if (sA == HI && sB == LO) {
      state = HILO;
      if (lohif) {
        ret = -1;
        lohif = false;
      }
    }
  }

  else if (state == HILO) {
    if (sA == HI && sB == HI) {
      state = HIHI;
      hilof = true;
      lohif = false;
    } else if (sA == LO && sB == LO) {
      state = LOLO;
      hilof = true;
      lohif = false;
    }
  }

  else if (state == LOLO) {
    if (sA == HI && sB == LO) {
      state = HILO;
      if (lohif) {
        ret = 1;
        lohif = false;
      }
    } else if (sA == LO && sB == HI) {
      state = LOHI;
      if (hilof) {
        ret = -1;
        hilof = false;
      }
    }
  }

  else { // state must be LOHI
    if (sA == HI && sB == HI) {
      state = HIHI;
      lohif = true;
      hilof = false;
    } else if (sA == LO && sB == LO) {
      state = LOLO;
      lohif = true;
      hilof = false;
    }
  }

  return ret;
}

int Scroller::getScrollThresholdA(void) {
  return scrollThresholdA;
}

int Scroller::al(void) {
  return lowA;
}

int Scroller::ah(void) {
  return highA;
}

int Scroller::getScrollThresholdB(void) {
  return scrollThresholdB;
}

int Scroller::calculateThresholdA(int curA) {
  if (curA < lowA)
    lowA = curA;
  else if (curA > highA)
    highA = curA;

  scrollThresholdA = ((highA - lowA) / 3) + lowA;
}

int Scroller::calculateThresholdB(int curB) {
  if (curB < lowB)
    lowB = curB;
  else if (curB > highB)
    highB = curB;

  scrollThresholdB = ((highB - lowB) / 3) + lowB;
}
