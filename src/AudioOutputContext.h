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
#ifndef _AudioOutputContext_h
#define _AudioOutputContext_h

#include <cstdint>

namespace kc1fsz {

/**
 * An abstract interface for the ability to play a continuous stream of 
 * PCM audio.
*/
class AudioOutputContext {
public:

    AudioOutputContext(uint32_t frameSize, uint32_t samplesPerSecond) :
        _frameSize(frameSize),
        _samplesPerSecond(samplesPerSecond) { }
    virtual ~AudioOutputContext() { }

    /**
     * This should be called continuously from the event loop, or at
     * least fast enough to keep up with the audio rate.
     */
    virtual void poll() = 0;
    
    virtual void play(int16_t* frame) = 0;

protected:

    uint32_t _frameSize;
    uint32_t _samplesPerSecond;
};

}

#endif
