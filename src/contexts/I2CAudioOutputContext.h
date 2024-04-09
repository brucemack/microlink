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

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/AudioOutputContext.h"

namespace kc1fsz {

class UserInfo;
class AudioAnalyzer;

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
        uint32_t bufferDepthLog2, int16_t* audioBuf,
        UserInfo* userInfo);
    virtual ~I2CAudioOutputContext();

    // Called from the 8 kHz timer ISR
    static void tickISR(void* obj) { static_cast<I2CAudioOutputContext*>(obj)->_tick(); }

    void setAnalyzer(AudioAnalyzer* aa) { _analyzer = aa; }

    uint32_t getTxFifoFull() const { return _txFifoFull; }

    bool getSquelch() const { return _squelchOpen; }

    // ----- From AudioOutputContext ------------------------------------------

    virtual void reset();

    // IMPORTANT: This assumes 16-bit PCM audio.  This function 
    // supports either 160 or 160x4 frames.
    virtual bool play(const int16_t* frame, uint32_t frameLen);

    virtual void run();

    virtual uint32_t getSyncErrorCount() { return _idleCount + _overflowCount; }

    // TODO: Consolidate in Synth
    virtual void tone(uint32_t freq, uint32_t durationMs);

private:
    
    void _play(int16_t pcm);
    void _openSquelchIfNecessary();
    void _tick();

    const uint32_t _bufferDepthLog2;
    const uint32_t _bufferMask;
    
    // How deep we should get before triggering the audio play
    uint32_t _triggerDepth;
    int16_t* _audioBuf;
    UserInfo* _userInfo;
    uint32_t _frameWriteCount;
    uint32_t _framePlayCount;
    // The pointer to the next sample to be played, w/in the current frame
    uint32_t _samplePtr;
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
    // Used to control squelch reporting
    bool _squelchOpen;
    // The last time we saw audio which can be used to 
    // manage squelch.
    timestamp _lastAudioTime;

    // Tone features
    volatile uint32_t _toneCount = 0;
    volatile bool _inTone = false;
    volatile float _ym1 = 0, _ym2 = 0, _a = 0;

    AudioAnalyzer* _analyzer = 0;
    volatile uint32_t _txFifoFull = 0;
};

}

#endif
