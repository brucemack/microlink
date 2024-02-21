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

#include "common.h"
#include "UserInfo.h"
#include "I2CAudioOutputContext.h"

namespace kc1fsz {

// How long we wait in silence before deciding that the audio
// output has stopped.
static const uint32_t SQUELCH_INTERVAL_MS = 1000;

I2CAudioOutputContext* I2CAudioOutputContext::_INSTANCE = 0;

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
    _timer.setIntervalUs(_intervalUs);
    _dacAddr = 0x60;
    _triggerDepth = (1 << _bufferDepthLog2) / 2;

    // Build a fixed table with an 800 Hz tone
    float omega = (2.0 * 3.14159 * 800.0) / (float)sampleRate;
    float phi = 0;
    for (unsigned int i = 0; i < _toneBufSize; i++) {
        _toneBuf[i] = 32767.0 * std::cos(phi);
        phi += omega;
    }

    _INSTANCE = this;
    /*
    // ---
    // Attempt to get a hardware clock
    hardware_alarm_claim(1);
    hardware_alarm_set_callback(1, _alarm);
    _nextTick = delayed_by_us(get_absolute_time(), 12500);
    hardware_alarm_set_target(1, _nextTick);
    // ---
    */

    reset();
}

I2CAudioOutputContext::~I2CAudioOutputContext() {    
    //hardware_alarm_unclaim(1);
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
    _toneStep = 0;
    _squelchOpen = false;
    _lastAudioTime = 0;
    _timer.reset();
    // Park DAC at middle of range
    _play(0);
}

void I2CAudioOutputContext::tone(uint32_t freq, uint32_t durationMs) {
    
    _toneCount = (durationMs * 8000 / 1000);
    _tonePtr = 0;
    _inTone = true;

    if (freq == 800) {
        _toneStep = 4;
    } else if (freq == 400) {
        _toneStep = 2;
    } else {
        _toneStep = 1;
    }

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

    // Pacing at the audio sample clock (ex: 8kHz)
    activity = _timer.poll();

    if (activity) {
        if (_inTone) {
            
            // TODO: HAVE A GAIN SETTING
            _play((_toneBuf[_tonePtr >> 2]) >> 2);
            // Move across tone, looking for wrap
            _tonePtr += _toneStep;
            if (_tonePtr == (_toneBufSize << 2)) {
                _tonePtr = 0;
            }   
            // Manage duration
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

    // Manage squelch off
    if (_squelchOpen && 
        (time_ms() > _lastAudioTime) &&
        (time_ms() - _lastAudioTime) > SQUELCH_INTERVAL_MS ) {
        _squelchOpen = false;
        _userInfo->setSquelchOpen(_squelchOpen);
        activity = true;
    }

    return activity;
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

void I2CAudioOutputContext::_openSquelchIfNecessary() {
    if (!_squelchOpen) {
        _squelchOpen = true;
        _userInfo->setSquelchOpen(_squelchOpen);
    }
}

void I2CAudioOutputContext::_play(int16_t sample) {
    uint16_t centeredSample = (sample + 32767);
    uint8_t buf[3];
    makeFrame0(buf, centeredSample);
    // TODO: MAKE THIS NON-BLOCKING
    i2c_write_blocking(i2c_default, _dacAddr, buf, 3, true);
}

/*
void I2CAudioOutputContext::_alarm(uint timer) {
    if (_INSTANCE) {
        _INSTANCE->_tick();
    }
}

void I2CAudioOutputContext::_tick() {

    // Re-schedule the alarm
    _nextTick = delayed_by_us(_nextTick, 125);
    while (hardware_alarm_set_target(1, _nextTick)) {
        _nextTick = delayed_by_us(_nextTick, 125);
    } 

    if (_inTone) {
        // TODO: HAVE A GAIN SETTING
        _play((_toneBuf[_tonePtr >> 2]) >> 2);
        // Move across tone, looking for wrap
        _tonePtr += _toneStep;
        if (_tonePtr == (_toneBufSize << 2)) {
            _tonePtr = 0;
        }   
        // Manage duration
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
*/
}
