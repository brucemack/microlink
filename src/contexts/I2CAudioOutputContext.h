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
#ifndef _I2CAudioOutputContext_h
#define _I2CAudioOutputContext_h

#include <cstdint>

#include "kc1fsz-tools/AudioOutputContext.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"

namespace kc1fsz {

/**
 * An implementation of the AudioOutputContext that assumes a
 * MCP4725 DAC on an I2C interface using the Pi Pico SDK.
*/
class I2CAudioOutputContext : public AudioOutputContext {
public:

    /**
     * @param frameSize The number of 16-bit samples in a frame. 
     * @param bufferDepthLog2 The number of frames that can fit in the 
     *   audio buffer (good to have about a second of back-log). The 
     *   depth must be a power of 2.
     * @param audioBuf Must be bufferDepth x frameSize in length.
    */
    I2CAudioOutputContext(uint32_t frameSize, uint32_t sampleRate, 
        uint32_t bufferDepthLog2, int16_t* audioBuf);
    virtual ~I2CAudioOutputContext();

    virtual void reset();

    // IMPORTANT: This assumes 16-bit PCM audio
    virtual bool play(const int16_t* frame);

    virtual bool poll();

    virtual uint32_t getSyncErrorCount() { return _idleCount + _overflowCount; }

private:

    void _play(int16_t pcm);

    uint32_t _bufferDepthLog2;
    // How deep we should get before triggering the audio play
    uint32_t _triggerDepth;
    int16_t *_audioBuf;
    uint32_t _frameWriteCount;
    uint32_t _framePlayCount;
    uint32_t _playPtr;
    // How much time to wait between sample output. This is a
    // variable to support adaptive approaches.
    uint32_t _intervalUs;
    uint8_t _dacAddr;
    // This keeps track of the number of sample intervals
    // where nothing was ready to play yet.
    uint32_t _idleCount;
    uint32_t _overflowCount;
    // This indicates whether we are actually playing sound
    // vs. sitting in silence.
    bool _playing;

    PicoPollTimer _timer;
};

}

#endif
