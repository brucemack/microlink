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

#include "kc1fsz-tools/Runnable.h"
#ifdef PICO_BUILD
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#endif

namespace kc1fsz {

class AudioProcessor;

/**
 * Demonstration audio source that does nothing more than tone generation.
*/
class TestAudioInputContext : public Runnable {
public:

    TestAudioInputContext(uint32_t frameSize, uint32_t samplesPerSecond);

    void setSink(AudioProcessor* sink) { _sink = sink; }

    void sendTone(uint32_t freq, uint32_t ms);

    bool run();

private:

    const uint32_t _frameSize;
    const uint32_t _sampleRate;
    AudioProcessor* _sink;
    bool _inTone;
    uint32_t _frameCount;
    float _amplitude;
    float _omega;
    // Maintain phase consistency
    float _phi;
    int16_t _toneFrame[160 * 4];

    // This timer is used to time the overall tone
#ifdef PICO_BUILD
    PicoPollTimer _toneTimer;
#endif

    // This timer is used to pace the generation of the frames
#ifdef PICO_BUILD
    PicoPollTimer _frameTimer;
#endif
};

}

#endif
