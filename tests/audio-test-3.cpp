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
#include <fstream>
#include <vector>

#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/wav_util.h"

#include "kc1fsz-tools/Common.h"

#include "common.h"
#include "contexts/W32AudioOutputContext.h"
#include "Prompts.h"

using namespace std;
using namespace kc1fsz;

// The size of one EchoLink RTP packet (after decoding)
const int frameSize = 160 * 4;
const int frameCount = 16;
// Double-buffer
int16_t audioData[frameCount * frameSize];
// Double-buffer
int16_t silenceData[2 * frameSize];

// A simple example of playing an audio tone using alternating buffers.
//
int main(int, const char**) {

    const int sampleRate = 8000;

    W32AudioOutputContext context(frameSize, sampleRate, audioData, silenceData);

    int16_t pcm[160 * 48];
    for (int i = 0; i < 160 * 48; i++) {
        pcm[i] = 0;
    }

    int soundIdx = 5;

    cout <<  SoundMeta[soundIdx].start << endl;
    cout <<  SoundMeta[soundIdx].length << endl;

    // Decode the sound into the PCM buffer   
    Decoder decoder;
    for (int f = 0; f < SoundMeta[soundIdx].length; f++) {
        Parameters params;
        PackingState state;
        params.unpack(SoundData + ((SoundMeta[soundIdx].start + f) * 33), &state);
        decoder.decode(&params, pcm + (f * 160));
    }
                
    int framePtr = 0;
    uint32_t lastFrameMs = 0;
    unsigned int gapMs = 80;

    // The test runs for some fixed period.  This is basically the main
    // event loop of the program.
    for (int i = 0; i < 400000; i++) {

        // Let the context do any background work it needs to
        context.run();

        uint32_t now = time_ms();

        if (now - lastFrameMs >= gapMs) {

            lastFrameMs = now;

            








            // Try to play
            if (framePtr < SoundMeta[soundIdx].length) {
                bool good = context.play(pcm + (framePtr * 160));
                if (good) {
                    framePtr += 4;
                }
            } else {
                framePtr = 0;
            }
        }

        Sleep(5);
    }
}
