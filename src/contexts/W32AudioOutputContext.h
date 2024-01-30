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
#ifndef _W32AudioOutputContext_h
#define _W32AudioOutputContext_h

#include <cstdint>
#include <Windows.h>
#include "../AudioOutputContext.h"

namespace kc1fsz {

class W32AudioOutputContext : public AudioOutputContext {
public:

    /**
     * @param bufferArea Must be 2xframeSize in length.
    */
    W32AudioOutputContext(uint32_t frameSize, uint32_t samplesPerSecond, 
        int16_t* bufferArea);
    virtual ~W32AudioOutputContext();

    virtual bool poll();

    virtual void play(int16_t* frame);

private:

    int16_t* _waveData;
    HANDLE _event;
    HWAVEOUT _waveOut;
    WAVEHDR _waveHdr[2];
    uint32_t _frameCount;
    uint32_t _frameCountOnLastWrite;
};

}

#endif
