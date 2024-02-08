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
#ifndef _PicoAudioInputContext_h
#define _PicoAudioInputContext_h

#include <cstdint>

#include "pico/util/queue.h"

#include "kc1fsz-tools/rp2040/PicoPollTimer.h"

namespace kc1fsz {

class AudioSink;

class PicoAudioInputContext {
public:

    PicoAudioInputContext(queue_t& queue);

    void setSink(AudioSink *sink);

    bool poll();

    void setPtt(bool keyed) { _keyed = keyed; }

private:

    // This is the queue used to pass ADC samples from the ISR and into the main 
    // event loop.
    queue_t& _queue;
    AudioSink* _sink;
    bool _keyed;
    PicoPollTimer _timer;
    int16_t _dcBias;
    int16_t _frameBuf[160 * 4];
    uint32_t _sampleCount;
};

}

#endif


