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
#include <iostream>
#include "hardware/adc.h"

// Physical pin 31
#define AUDIO_IN_PIN (26)

#include "kc1fsz-tools/AudioProcessor.h"
#include "PicoAudioInputContext.h"

using namespace std;

namespace kc1fsz {

int PicoAudioInputContext::traceLevel = 0;

PicoAudioInputContext* PicoAudioInputContext::INSTANCE = 0;

void PicoAudioInputContext::setup() {

    // Get the ADC initialized
    adc_gpio_init(AUDIO_IN_PIN);
    adc_init();
    uint8_t adcChannel = 0;
    adc_select_input(adcChannel);
    adc_fifo_setup(
        true,   
        false,
        1,
        false,
        false
    );
    adc_set_clkdiv(_adcClockHz / _audioSampleRate);
    irq_set_exclusive_handler(ADC_IRQ_FIFO, PicoAudioInputContext::_adc_irq_handler);    
    adc_irq_set_enabled(true);
    irq_set_enabled(ADC_IRQ_FIFO, true);
}

// Decorates a function name, such that the function will execute from RAM 
// (assuming it is not inlined into a flash function by the compiler)
void __not_in_flash_func(PicoAudioInputContext::_adc_irq_handler) () {  
    if (INSTANCE) {
        INSTANCE->_interruptHandler();
    }
}

PicoAudioInputContext::PicoAudioInputContext() 
:   _keyed(false),
    _statsPtr(0) {

    if (INSTANCE != 0) {
        panic("Init error");
    }

    INSTANCE = this;
}

void PicoAudioInputContext::setPtt(bool keyed) { 

    _keyed = keyed; 

    // Turn on/off the ADC to minimize interrupt activity when not 
    // actually transmitting.
    adc_run(_keyed);
}

void __not_in_flash_func(PicoAudioInputContext::_interruptHandler)() {
    // We clear as much as possible on every interrupt
    while (!adc_fifo_is_empty()) {
        uint32_t slot = _audioInBufWriteCount.get() & _audioInBufDepthMask;
        // This will be a number from 0->4095 (12 bits).  
        // We are using a 32-bit integer to implement saturation
        int32_t sample = adc_fifo_get();
        // Center to give a number from -2048->2047 (12 bits)
        sample -= 2048;
        // Small tweaks
        sample += _dcBias;
        // Apply a gain and shift up to form 16-bit PCM.
        sample *= _gain;
        // Saturate to 16 bits
        if (sample > 32767) {
            sample = 32767;
        } else if (sample < -32768) {
            sample = -32768;
        }
        _audioInBuf[slot][_audioInBufWritePtr++] = (int16_t)sample;
        // Check to see if we've reached the end of the 4xframe
        if (_audioInBufWritePtr == _audioFrameSize * _audioFrameBlockFactor) {
            _audioInBufWritePtr = 0;
            if (_audioInBufWriteCount.get() >= _audioInBufReadCount.get()) {
                // Figure out how many slots have been used
                uint32_t used = _audioInBufWriteCount.get() - _audioInBufReadCount.get();
                // Check to see if there is room before wrapping onto the reader
                if (used < _audioInBufDepth - 1) {
                    _audioInBufWriteCount.inc();
                } else {
                    // In this case we just start writing on the same 4xframe
                    // again and lose the previous values.
                    _audioInBufOverflow++;
                }
            } 
            else {
                panic("Sanity check 1");
            }
        }
    }
}

int16_t PicoAudioInputContext::getAverage() const {
    int16_t r = 0;
    for (uint32_t s = 0; s < _statsSize; s++) 
        r += _stats[s].avg;
    return r / _statsSize;
}

int16_t PicoAudioInputContext::getMax() const {
    int16_t r = 0;
    for (uint32_t s = 0; s < _statsSize; s++) 
        r = std::max(_stats[s].max, r);
    return r;
}

int16_t PicoAudioInputContext::getClips() const {
    int16_t r = 0;
    for (uint32_t s = 0; s < _statsSize; s++) 
        r += _stats[s].clips;
    return r;
}

void PicoAudioInputContext::_updateStats(int16_t* audio) {

    _stats[_statsPtr].avg = 0;
    _stats[_statsPtr].max = 0;
    _stats[_statsPtr].clips = 0;
    
    for (uint32_t i = 0; i < 160 * 4; i++) {
        int16_t sample = audio[i];
        _stats[_statsPtr].avg += sample;
        _stats[_statsPtr].max = std::max(_stats[_statsPtr].max,
            (int16_t)std::abs(sample));
        if (sample == 32767 || sample == -32768) {
            _stats[_statsPtr].clips++;
        }
    }

    _stats[_statsPtr].avg /= (160 * 4);
    
    _statsPtr++;
    if (_statsPtr == _statsSize) {
        _statsPtr = 0;
    }
}

bool PicoAudioInputContext::run() {

    bool activity = false;

    // Check to see if we have data availble 
    if (_audioInBufWriteCount.get() > _audioInBufReadCount.get()) {
        activity = true;

        uint32_t slot = _audioInBufReadCount.get() & _audioInBufDepthMask;

        // (This is optional)
        _updateStats(_audioInBuf[slot]);
        
        if (_keyed) {
            // Pull down a 4xframe and try to move it into the transmitter
            bool accepted = _sink->play(_audioInBuf[slot]);
            if (accepted) {
                // Move the read pointer forward (we already know that 
                // the write counter is larger than the read counter)
                _audioInBufReadCount.inc();
            }
        } 
        // If the transmitter isn't keyed then just discard the input audio
        else {
            _audioInBufReadCount.inc();
        }
    }
    else if (_audioInBufWriteCount.get() < _audioInBufReadCount.get()) {
        panic("Sanity check 2");
    }

    return activity;
}

}
