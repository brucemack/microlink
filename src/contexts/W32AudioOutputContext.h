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

#include "kc1fsz-tools/AudioOutputContext.h"
#include "kc1fsz-tools/win32/Win32PerfTimer.h"

namespace kc1fsz {

class W32AudioOutputContext : public AudioOutputContext {
public:

    static int traceLevel;

    /**
     * @param bufferArea Must be 16 x frameSize in length.
     * @param silenceArea Must be 2 x frameSize in length.  
    */
    W32AudioOutputContext(uint32_t frameSize, uint32_t samplesPerSecond, 
        int16_t* audioArea, int16_t* silenceArea);
    virtual ~W32AudioOutputContext();

    virtual bool poll();
    virtual void play(int16_t* frame);

    virtual uint32_t getAudioQueueUsed() const {
        return _audioQueueUsed;
    }

private:

    HANDLE _event;
    HWAVEOUT _waveOut;

    bool _inSilence;

    int16_t* _audioData;
    // This controls how much audio backlog we can retain before
    // overflowing.
    static const uint32_t _audioQueueSize = 16;
    WAVEHDR _audioHdr[_audioQueueSize];
    uint32_t _audioQueuePtr;
    uint32_t _audioQueueUsed;

    int16_t* _silenceData;
    WAVEHDR _silenceHdr[2];
    uint32_t _silenceQueuePtr;

    Win32PerfTimer _timer;
};

}

#endif
