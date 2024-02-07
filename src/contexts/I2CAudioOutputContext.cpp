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
#include "hardware/i2c.h"

#include "I2CAudioOutputContext.h"

namespace kc1fsz {

I2CAudioOutputContext::I2CAudioOutputContext(uint32_t frameSize, uint32_t sampleRate, 
    int16_t* audioBuf) 
:   AudioOutputContext(frameSize, sampleRate),
    _audioBuf(audioBuf),
    _frameWriteCount(0),
    _framePlayCount(0),
    _playPtr(0),
    _intervalUs(125),
    _idleCount(0),
    _overflowCount(0)
{
    _timer.setIntervalUs(_intervalUs);
    _dacAddr = 0x60;
}

I2CAudioOutputContext::~I2CAudioOutputContext() {    
}

void I2CAudioOutputContext::reset() {
    _frameWriteCount = 0;
    _framePlayCount = 0;
    _playPtr = 0;
    _idleCount = 0;
    _overflowCount = 0;
    _timer.reset();
}

bool I2CAudioOutputContext::poll() {    
    // Pacing at 8kHz
    bool activity = _timer.poll();
    if (activity) {
        // This is what keep us from getting ahead
        // TODO: Make sure we don't hang forever on the wrap-aound case
        if (_frameWriteCount > _framePlayCount) {
            uint32_t factor = _framePlayCount & 1;
            const int16_t* start = _audioBuf + (_frameSize * factor);
            _play(start[_playPtr++]);
            if (_playPtr == _frameSize) {
                _framePlayCount++;
                _playPtr = 0;
            }
        } else {
            _idleCount++;
        }
    }
    return activity;
}

void I2CAudioOutputContext::play(int16_t* frame) {
    // Figure out starting point in double-buffer
    uint32_t factor = _frameWriteCount & 1;
    int16_t* start = _audioBuf + (_frameSize * factor);
    // Copy the data into the double-buffer
    for (uint32_t i = 0; i < _frameSize; i++) {
        start[i] = frame[i];
    }
    _frameWriteCount++;
}

/*
Assuming the raw PCM data looks like this:

              High Byte                      Low Byte
| 7   6   5   4   3   2   1   0  |  7   6   5   4   3   2   1   0  |
  b15 b14 b13 b12 b11 b10 b9  b8    b7  b6  b5  b4  b3  b2  b1  b0

The MCP4725 can only deal with 12 bits of significance, so we'll 
ignore bits b3-b0 on the input (those might be zero anyhow). Using
the labeling from the MCP4725 datasheet we have these bits:

| d11 d10 d9  d8  d7  d6  d5  d4 |  d3  d2  d1  d0  0   0   0   0  |

Which is convenient because that is exactly the format that they
specify for the second and third byte of the transfer.

See https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf 
(page 25). The bits are aligned in the same way, once you 
consider
*/

static void makeFrame0(uint8_t* buf, uint16_t data) {
    buf[0] = 0x40;
    buf[1] = (data >> 8) & 0xff;
    buf[2] = (uint8_t)(data & 0xf0);
}

static void makeFrame1(uint8_t* buf, uint16_t data) {
    buf[0] = 0x0f & (data >> 8);
    buf[1] = data & 0xff;
}

void I2CAudioOutputContext::_play(int16_t sample) {
    uint16_t centeredSample = (sample + 32767);
    uint8_t buf[3];
    makeFrame0(buf, centeredSample);
    // TODO: MAKE THIS NON-BLOCKING
    i2c_write_blocking(i2c_default, _dacAddr, buf, 3, true);
}

}

