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
 */
#include <cstring>

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/AudioProcessor.h"
#include "gsm-0610-codec/Parameters.h"

#include "common.h"

#include "Prompts.h"
#include "Synth.h"

using namespace std;

namespace kc1fsz {

Synth::Synth() 
:   _running(false) {
}

void Synth::generate(const char* str) {
    strcpyLimited(_str, str, _strSize);
    _strLen = strlen(_str);
    _strPtr = 0;
    _framePtr = 0;
    _running = true;
    _workingFrameReady = false;
    _decoder.reset();
}

void Synth::run() {

    if (!_running) {
        return;
    }

    int s = Sound::findSound(_str[_strPtr]);

    // Setup the next frame to be played, if necessary
    if (!_workingFrameReady) {
        // We load 4 frames at a time, padding with zeros as needed
        for (unsigned int f = 0; f < 4; f++) {
            if (_framePtr < SoundMeta[s].getFrameCount()) {
                Parameters params;
                PackingState state;
                uint8_t gsmFrame[33];
                SoundMeta[s].getGSMFrame(_framePtr, gsmFrame);
                params.unpack(gsmFrame, &state);
                _decoder.decode(&params, _workingFrame + (f * 160));
            }
            else {
                for (int i = 0; i < 160; i++) {
                    _workingFrame[(f * 160) + i] = 0;
                }
            }
            _framePtr++;
        }
        _workingFrameReady = true;
    }

    // Attempt to play some sound
    if (_workingFrameReady) {
        // This will return false if the player is busy
        if (_sink->play(_workingFrame, _frameSize)) {
            // Signal that we need to make a new one
            _workingFrameReady = false;
            // All frames in the sound played?
            if (_framePtr >= SoundMeta[s].getFrameCount()) {
                _strPtr++;
                _framePtr = 0;
                // No decoding continuity between sounds
                _decoder.reset();
                // Have we played the entire string?
                if (_strPtr == _strLen) {
                    _running = false;
                }
            }
        }
    }
}

}
