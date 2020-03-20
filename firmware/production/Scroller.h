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

#ifndef SCROLLER_AR_SIZE
#define SCROLLER_AR_SIZE 31
#endif

#ifndef SCROLLER_h
#define SCROLLER_h

class Scroller {
  private:
    const int THRESH_RANGE_LIM = 10;
    enum State { HIHI, HILO, LOLO, LOHI };
    State state;
    bool lohif;
    bool hilof;
    int lowA;
    int highA;
    bool cLowA;
    bool cHighA;
    int lowIndexA;
    int highIndexA;
    bool lowOverflowA;
    bool highOverflowA;
    int lowB;
    int highB;
    bool cLowB;
    bool cHighB;
    int lowIndexB;
    int highIndexB;
    bool lowOverflowB;
    bool highOverflowB;
    int scrollThresholdA;
    int scrollThresholdB;
    int arLowA[SCROLLER_AR_SIZE];
    int arHighA[SCROLLER_AR_SIZE];
    int arLowB[SCROLLER_AR_SIZE];
    int arHighB[SCROLLER_AR_SIZE];
    int calculateThresholdA(int);
    int calculateThresholdB(int);
    int calculateThreshold(int, int&, int&, bool&, bool&, int*, int*, int&, int&, bool&, bool&);
    int thresholdEquation(int, int);
    void incrementIndex(int&, bool&);
  public:
    Scroller(void);
    int scroll(int, int);
    void debug(void);
};

#endif
