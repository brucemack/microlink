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
#include <cmath>

#include "hardware/i2c.h"
#include "kc1fsz-tools/AudioAnalyzer.h"

#include "common.h"
#include "UserInfo.h"
#include "I2CAudioOutputContext.h"

namespace kc1fsz {

// How long we wait in silence before deciding that the audio
// output has stopped.
static const uint32_t SQUELCH_INTERVAL_MS = 1000;

I2CAudioOutputContext::I2CAudioOutputContext(uint32_t frameSize, uint32_t sampleRate,
    uint32_t bufferDepthLog2, int16_t* audioBuf,
    UserInfo* userInfo) 
:   AudioOutputContext(frameSize, sampleRate),
    _bufferDepthLog2(bufferDepthLog2),
    // For example: a depth of 16 means a depthLog2 of 4.  
    // The mask is 1, 10, 100, 1000, 10000, minus 1 -> 1111
    _bufferMask((1 << bufferDepthLog2) - 1),
    _audioBuf(audioBuf),
    _userInfo(userInfo),
    _frameWriteCount(0),
    _framePlayCount(0),
    _samplePtr(0),
    _intervalUs(125),
    _idleCount(0),
    _overflowCount(0),
    _playing(false),
    _squelchOpen(false),
    _lastAudioTime(0)
{
    _dacAddr = 0x60;
    _triggerDepth = (1 << _bufferDepthLog2) / 2;

    /*        
    // Build a fixed table with an 800 Hz tone
    float omega = (2.0 * 3.14159 * 800.0) / (float)sampleRate;
    float phi = 0;
    for (unsigned int i = 0; i < _toneBufSize; i++) {
        _toneBuf[i] = 32767.0 * std::cos(phi);
        phi += omega;
    }
    */

    // TODO: Pass in I2C port
    // One-time initialization of the I2C channel
    i2c_hw_t *hw = i2c_get_hw(i2c_default);
    hw->enable = 0;
    hw->tar = _dacAddr;
    hw->enable = 1;

    reset();
}

I2CAudioOutputContext::~I2CAudioOutputContext() {    
}

void I2CAudioOutputContext::reset() {
    _frameWriteCount = 0;
    _framePlayCount = 0;
    _samplePtr = 0;
    _idleCount = 0;
    _overflowCount = 0;
    _playing = false;
    _inTone = false;
    _toneCount = 0;
    _squelchOpen = false;
    _lastAudioTime = 0;
    // Park DAC at middle of range
    _play(0);
}

void I2CAudioOutputContext::tone(uint32_t freq, uint32_t durationMs) {
    
    _toneCount = (durationMs * 8000 / 1000);

    // See: https://dspguru.com/dsp/tricks/sine_tone_generator/
    _ym1 = 0;
    _ym2 = -std::sin(2.0 * 3.1415926 * ((float)freq / 8000.0));
    _a = 2.0 * std::cos(2.0 * 3.1415926 * ((float)freq / 8000.0));
    _inTone = true;

    _openSquelchIfNecessary();

    // Keep track of time for squelch purposes
    _lastAudioTime = time_ms() + durationMs;
}

bool I2CAudioOutputContext::play(const int16_t* frame) {

    // Check for the full situation and push back if necessary
    if (((_frameWriteCount + 1) & _bufferMask) ==
        (_framePlayCount & _bufferMask)) {
        return false;
    }

    // Figure out which slot to use in the rotating buffer
    uint32_t slot = _frameWriteCount & _bufferMask;
    int16_t* start = _audioBuf + (_frameSize * slot);

    // Copy the data into the slot
    bool nonZeroSample = false;
    for (uint32_t i = 0; i < _frameSize; i++) {
        if (_analyzer) {
            _analyzer->sample(frame[i]);
        }
        start[i] = frame[i];
        if (frame[i] != 0) {
            nonZeroSample = true;
        }
    }

    _frameWriteCount++;

    if (nonZeroSample) {
        _openSquelchIfNecessary();
        // Keep track of time for squelch purposes
        _lastAudioTime = time_ms();
    }

    // Look for the wrap-around case (rare)
    if (_frameWriteCount == 0) {
        // Move way from the discontinuity
        _frameWriteCount += (_frameSize * 2);
        _framePlayCount += (_frameSize * 2);
    }

    return true;
}

bool I2CAudioOutputContext::run() {    

    bool activity = false;

    // Manage squelch off
    if (_squelchOpen && 
        // Watch out, sometimes the _lastAudioTime is in the future
        (time_ms() > _lastAudioTime) &&
        (time_ms() - _lastAudioTime) > SQUELCH_INTERVAL_MS ) {
        _squelchOpen = false;
        _userInfo->setSquelchOpen(_squelchOpen);
        activity = true;
    }

    return activity;
}

void I2CAudioOutputContext::_openSquelchIfNecessary() {
    if (!_squelchOpen) {
        _squelchOpen = true;
        _userInfo->setSquelchOpen(_squelchOpen);
    }
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
void I2CAudioOutputContext::_play(int16_t sample) {

    // This was measured to take 310ns

    // Go from 16-bit PCM -32768->32767 to 12-bit PCM 0->4095
    uint16_t centeredSample = (sample + 32767);
    uint16_t rawSample = centeredSample >> 4;

    i2c_hw_t *hw = i2c_get_hw(i2c_default);
    // Tx FIFO must not be full
    assert(hw->status & I2C_IC_STATUS_TFNF_BITS); 

    // To create an output sample we need to write three words.  The STOP flag
    // is set on the last one.
    //
    // 0 0 0 | 0   1   0   x   x   0   0   x 
    // 0 0 0 | d11 d10 d09 d08 d07 d06 d05 d04
    // 0 1 0 | d03 d02 d01 d00 x   x   x   x
    //   ^
    //   |
    //   +------ STOP BIT!
    //
    hw->data_cmd = 0b000'0100'0000;
    hw->data_cmd = 0b000'0000'0000 | ((rawSample >> 4) & 0xff); // High 8 bits
    // STOP requested.  Data is low 4 bits of sample, padded on right with zeros
    hw->data_cmd = 0b010'0000'0000 | ((rawSample << 4) & 0xff); 
}

void I2CAudioOutputContext::_tick() {

    if (_inTone) {

        // NOTE: Floating point in an ISR?
        float ym0 = (_a * _ym1) - _ym2;
        _ym2 = _ym1;
        _ym1 = ym0;

        _play(ym0 * 32767.0);

        // Manage duration of tone
        _toneCount--;
        if (_toneCount == 0) {
            _inTone = false;
            // Park DAC at middle of range
            _play(0);
        }
    }
    else if (!_playing) {
        // Decide whether to start playing
        if ((_frameWriteCount > _framePlayCount) &&
            (_frameWriteCount - _framePlayCount) > _triggerDepth) {
            _playing = true;
        }
    } 
    else {
        if (_frameWriteCount > _framePlayCount) {

            uint32_t slot = _framePlayCount & _bufferMask;
            const int16_t* start = _audioBuf + (_frameSize * slot);

            _play(start[_samplePtr++]);

            // Have we played the entire frame?
            if (_samplePtr == _frameSize) {
                _samplePtr = 0;
                _framePlayCount++;

                // Look for the wrap-around case (rare)
                if (_framePlayCount == 0) {
                    // Move way from the discontinuity
                    _frameWriteCount += (_frameSize * 2);
                    _framePlayCount += (_frameSize * 2);
                }
            }
        } else {
            _playing = false;
            _idleCount++;
            // Park DAC at middle of range
            _play(0);
        }
    }
}

}
