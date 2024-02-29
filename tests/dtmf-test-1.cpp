/**
 * MicroLink EchoLink Station
 * Copyright (C) 2024, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * FOR AMATEUR RADIO USE ONLY.
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 * 
 * =================================================================================
 * This file is unit-test code only.  None of this should be use for 
 * real applications!
 * =================================================================================
 */
#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>

#include "kc1fsz-tools/DTMFDetector.h"

using namespace std;
using namespace kc1fsz;

int main(int, const char**) {

    const int sampleRate = 8000;
    const int frameSize = 160;

    const uint32_t historySize = 2048;
    int16_t history[historySize];

    DTMFDetector det(history, historySize, sampleRate);

    
    const int toneR0 = 697;
    const int toneR1 = 770;
    //const int tone = 852;
    //const int tone = 941;
    //const int tone0 = 941;
    
    const int toneC1 = 1209;
    const int toneC2 = 1336;

    int16_t frame[frameSize];

    // Fill buffers with a consistent tone.

    float freqRad = (float)toneR1 * 2.0 * 3.1415926;
    float omega = freqRad / (float)sampleRate;
    float phi = 0;
    for (int i = 0; i < frameSize; i++) {
        frame[i] = 32767.0 * 0.25 * std::cos(phi);
        phi += omega;
    }

    freqRad = (float)toneC1 * 2.0 * 3.1415926;
    omega = freqRad / (float)sampleRate;
    phi = 0;
    for (int i = 0; i < frameSize; i++) {
        frame[i] += 32767.0 * 0.25 * std::cos(phi);
        phi += omega;
    }

    det.play(frame, frameSize);

    det.play(frame, frameSize);

    // Add some spurious power
    freqRad = (float)(toneR0) * 2.0 * 3.1415926;
    omega = freqRad / (float)sampleRate;
    phi = 0;
    for (int i = 0; i < frameSize; i++) {
        frame[i] += 32767.0 * 0.25 * std::cos(phi);
        phi += omega;
    }

    det.play(frame, frameSize);
    det.play(frame, frameSize);

    if (det.resultAvailable()) {
        cout << "GOT " << det.getResult() << endl;
    } else {
        cout << "No result" << endl;
    }

    return 0;
}
