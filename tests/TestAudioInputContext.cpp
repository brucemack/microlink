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
#include <iostream>
#include <cmath>

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/AudioSink.h"

#include "common.h"
#include "TestAudioInputContext.h"

using namespace std;

namespace kc1fsz {

class AudioSink;

TestAudioInputContext::TestAudioInputContext(uint32_t frameSize, uint32_t sampleRate)
:   _frameSize(frameSize),
    _sampleRate(sampleRate),
    _inTone(false) {
}

bool TestAudioInputContext::poll() {

    if (_inTone) {
        if (_toneTimer.poll()) {
            _inTone = false;
            return true;
        }
        else if (_frameTimer.poll()) {
            if (_sink->play(_toneFrame)) {
                _frameCount++;
            } else {
                cout << "Frame rejected: " << _frameCount << endl;
            }
            return true;
        }
    }

    return false;
}

void TestAudioInputContext::sendTone(uint32_t freq, uint32_t ms) {
    
    _inTone = true;
    _frameCount = 0;
    _omega = 2.0 * 3.141526 * (float)freq / (float)_sampleRate;
    _phi = 0;
    _amplitude = 0.5;

    // Pre-build the tone
    for (unsigned int i = 0; i < _frameSize; i++) {
        _toneFrame[i] = 32767.0 * _amplitude * std::cos(_phi);
        _phi += _omega;
    }

    // TODO: REMOVE HC
    // NOTE: SLIGHTLY FAST?
    _frameTimer.setIntervalUs(80 * 1000);
    _toneTimer.setIntervalUs(ms * 1000);
}

}
