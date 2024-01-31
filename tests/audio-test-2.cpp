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
//#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>

#include "../src/common.h"
#include "../src/contexts/W32AudioOutputContext.h"

using namespace std;
using namespace kc1fsz;

// The size of one EchoLink RTP packet (after decoding)
const int frameSize = 160 * 4;
const int frameCount = 8;
// Double-buffer
int16_t audioData[frameCount * frameSize];
// Double-buffer
int16_t silenceData[2 * frameSize];

// A simple example of playing an audio tone using alternating buffers.
//
int main(int, const char**) {

    const int sampleRate = 8000;

    W32AudioOutputContext context(frameSize, sampleRate, audioData, silenceData);

    uint32_t lastFrameMs = 0;

    // The test runs for some fixed period.  This is basically the main
    // event loop of the program.
    for (int i = 0; i < 400; i++) {

        // Let the context do any background work it needs to
        context.poll();

        unsigned int gapMs = 80;

        // Simulate a slowdown
        //if (i > 300 && i < 350) {
        //    cout << "Slow down" << endl;
        //    gapMs = 100;
        //}

        // Simulate the generation of a new audio frame every 20ms
        uint32_t now = time_ms();
        if (now - lastFrameMs >= gapMs) {

            lastFrameMs = now;

            int16_t frame[frameSize];

            // Fill buffers with a consistent tone.
            float freqRad = 1000.0 * 2.0 * 3.1415926;
            float omega = freqRad / (float)sampleRate;
            float phi = 0;
            for (int i = 0; i < frameSize; i++) {
                frame[i] = 32767.0 * std::cos(phi);
                phi += omega;
            }

            context.play(frame);
        }

        Sleep(5);
    }
}
