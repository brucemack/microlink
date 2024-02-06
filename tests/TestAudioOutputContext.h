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
#ifndef _TestAudioOutputContext_h
#define _TestAudioOutputContext_h

#include <iostream>
#include "kc1fsz-tools/AudioOutputContext.h"

namespace kc1fsz {

/**
 * A dummy implementation that does nothing.
 */
class TestAudioOutputContext : public AudioOutputContext {
public:

    TestAudioOutputContext(uint32_t frameSize, uint32_t samplesPerSecond)
    : AudioOutputContext(frameSize, samplesPerSecond) { }

    bool poll() { return false; }

    void play(int16_t* frame) { 
        std::cout << "<Audio Frame>" << std::endl;
    }
};

}

#endif
