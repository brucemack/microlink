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
#ifndef _PicoAudioInputContext_h
#define _PicoAudioInputContext_h

#include <cstdint>

#include "hardware/adc.h"

#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"

#include "AtomicInteger.h"

namespace kc1fsz {

class AudioProcessor;
class AudioAnalyzer;

class PicoAudioInputContext : public Runnable {
public:

    static int traceLevel;
    static PicoAudioInputContext* INSTANCE;

    static void setup();

    PicoAudioInputContext();
    virtual ~PicoAudioInputContext() { }

    void setSink(AudioProcessor *sink) { _sink = sink; }

    void setSampleCb(void (*sampleCb)(void*), void* sampleCbData) {
        _sampleCb = sampleCb;
        _sampleCbData = sampleCbData;
    }

    void setADCEnabled(bool en);
    
    uint32_t getOverflowCount() const { return _audioInBufOverflow; }
    void resetOverflowCount() { _audioInBufOverflow = 0; }

    int16_t getBias() const { return _dcBias; }

    /**
     * The DC bias is added to every sample.  It is used to compensate for 
     * centering inaccuracies in the circuit prior to the ADC.
     */
    void addBias(int16_t b) { _dcBias += (b >> 4); }

    void setAnalyzer(AudioAnalyzer* aa) { _analyzer = aa; }

    uint32_t getMaxSkew() const { return _maxSkew; }
    uint32_t getMaxLen() const { return _maxLen; }
    void resetMax() { _maxSkew = 0; _maxLen = 0; }

    // ----- From Runnable ---------------------------------------------------

    virtual bool run();

private:   

    static void _adc_irq_handler();

    void _interruptHandler();

    AudioProcessor* _sink = 0;
    void (*_sampleCb)(void*) = 0;
    void* _sampleCbData = 0;

    static const uint32_t _adcClockHz = 48000000;
    static const uint32_t _audioSampleRate = 8000;
    static const uint32_t _audioFrameSize = 160;
    static const uint32_t _audioFrameBlockFactor = 4;
    // This is a buffer where the data sampled from the ADC is staged on the
    // way to the TX chain. As usual, we process 4 audio frames at a time.
    static const uint32_t _audioInBufDepth = 2;
    static const uint32_t _audioInBufDepthMask = 0x01;
    int16_t _audioInBuf[_audioInBufDepth][_audioFrameSize * _audioFrameBlockFactor];
    // Keeps track of how many 4xframes have been written. The ISR
    // is the ONLY WRITER of this value.
    AtomicInteger _audioInBufWriteCount;
    // Keeps track of how many 4xframes have been read. The main poll() loop
    // is the ONLY WRITER of this value.
    AtomicInteger _audioInBufReadCount;
    // Write pointer - ONLY ACCESSED BY ISR!
    uint32_t _audioInBufWritePtr = 0;
    // Keep count of overflows/underflows
    uint32_t _audioInBufOverflow = 0;
    // Used to trim the centering
    // TODO: MAKE THIS ADJUSTABLE
    int16_t _dcBias = 100;
    // This includes x16 for 12 to 16 bit PCM conversion and a gain
    // of 0.5.
    int16_t _gain = 16;

    bool _adcEnabled = false;

    PicoPerfTimer _perfTimer;
    volatile uint32_t _maxSkew = 0;
    volatile uint32_t _maxLen = 0;

    AudioAnalyzer* _analyzer = 0;
};

}

#endif
