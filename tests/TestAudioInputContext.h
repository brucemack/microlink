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
#ifndef _TestAudioInputContext_h
#define _TestAudioInputContext_h

#include <iostream>

namespace kc1fsz {

class AudioSink;

/**
 * Demonstration audio source that does nothing more than tone generation.
*/
class TestAudioInputContext {
public:

    TestAudioInputContext(uint32_t frameSize, uint32_t samplesPerSecond);

    bool poll();

    void setSink(AudioSink* sink) { _sink = sink; }

    void sendTone(uint32_t freq, uint32_t ms);

private:

    const uint32_t _frameSize;
    const uint32_t _sampleRate;
    AudioSink* _sink;
    bool _inTone;
    uint32_t _nextFrameMs;
    uint32_t _toneEndMs;
    uint32_t _frameCount;
    float _omega;
    // Maintain phase consistency
    float _phi;
};

}

#endif
