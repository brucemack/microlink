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
#include "hardware/adc.h"

#include "kc1fsz-tools/AudioSink.h"
#include "PicoAudioInputContext.h"

namespace kc1fsz {

PicoAudioInputContext::PicoAudioInputContext(queue_t& queue)
:   _queue(queue),
    _sampleCount(0) {    
}

void PicoAudioInputContext::setSink(AudioSink* sink) {
    _sink = sink;
}

bool PicoAudioInputContext::poll() {

    bool activity = false;

    // Grab as much as possible from the queue in each cycle
    while (!queue_is_empty(&_queue)) {
        activity = true;
        // Pull off ADC queue - will be a value from 0->4095
        int16_t lastSample = 0;
        queue_remove_blocking(&_queue, &lastSample);
        if (_keyed) {
            // Center around zero
            lastSample -= 2048;
            // Scale up to a 16-bit PCM
            lastSample = lastSample * 16;
            _frameBuf[_sampleCount++] = lastSample;
            // When a full frame is collected pass it along
            if (_sampleCount == 160 * 4) {
                _sink->play(_frameBuf);
                _sampleCount = 0;
            }
        }
    }

    return activity;
}

}
