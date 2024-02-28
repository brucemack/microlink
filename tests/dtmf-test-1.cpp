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

    int16_t frame[frameSize];

    // Fill buffers with a consistent tone.
    float freqRad = 697.0 * 2.0 * 3.1415926;
    float omega = freqRad / (float)sampleRate;
    float phi = 0;
    for (int i = 0; i < frameSize; i++) {
        frame[i] = 32767.0 * std::cos(phi);
        phi += omega;
    }

    return 0;
}
