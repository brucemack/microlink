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

#include "kc1fsz-tools/AudioSink.h"
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
    adc_run(true);
}

// Decorates a function name, such that the function will execute from RAM 
// (assuming it is not inlined into a flash function by the compiler)
void __not_in_flash_func(PicoAudioInputContext::_adc_irq_handler) () {  
    if (INSTANCE) {
        INSTANCE->_interruptHandler();
    }
}

PicoAudioInputContext::PicoAudioInputContext() {

    if (INSTANCE != 0) {
        panic("Init error");
    }

    INSTANCE = this;
}

void PicoAudioInputContext::setSink(AudioSink* sink) {
    _sink = sink;
}

void __not_in_flash_func(PicoAudioInputContext::_interruptHandler)() {
    // We clear as much as possible on every interrupt
    while (!adc_fifo_is_empty()) {
        uint32_t slot = _audioInBufWriteCount.get() & _audioInBufDepthMask;
        // This will be a number from 0->4095 (12 bits)
        int16_t sample = adc_fifo_get();
        // Center to give a number from -2048->2047 (12 bits)
        sample -= 2048;
        // Small tweaks
        sample += _dcBias;
        // Apply a gain and shift up to form 16-bit PCM.
        sample *= _gain;
        _audioInBuf[slot][_audioInBufWritePtr++] = sample;
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
                    // again and loose the previous values.
                    _audioInBufOverflow++;
                }
            } 
            else {
                panic("Sanity check");
            }
        }
    }
}

bool PicoAudioInputContext::poll() {

    bool activity = false;

    // Check to see if we have data availble 
    if (_audioInBufWriteCount.get() > _audioInBufReadCount.get()) {
        activity = true;
        if (_keyed) {
            uint32_t slot = _audioInBufReadCount.get() & _audioInBufDepthMask;
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
        panic("Sanity check");
    }

    return activity;
}

}
