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
#include <algorithm>

#include "hardware/adc.h"

#include "kc1fsz-tools/AudioProcessor.h"
#include "kc1fsz-tools/AudioAnalyzer.h"

#include "PicoAudioInputContext.h"

// Physical pin 31
#define AUDIO_IN_PIN (26)

using namespace std;

namespace kc1fsz {

int PicoAudioInputContext::traceLevel = 0;

PicoAudioInputContext* PicoAudioInputContext::INSTANCE = 0;

void PicoAudioInputContext::setup() {

    // Get the ADC pin initialized
    adc_gpio_init(AUDIO_IN_PIN);
    adc_init();
    uint8_t adcChannel = 0;
    adc_select_input(adcChannel);    
    adc_fifo_setup(
        // Enable
        true,   
        // DREQ not enabled
        false,
        // DREQ threshold (but assuming this also applies to INT)
        1,
        // If enabled, bit 15 of the FIFO contains error flag for each sample
        false,
        // Shift FIFO contents to be one byte in size (for byte DMA) - enables 
        // DMA to byte buffers.
        false
    );
    // Divide clock to 8 kHz
    adc_set_clkdiv(_adcClockHz / _audioSampleRate);
    // Install INT handler
    irq_set_exclusive_handler(ADC_IRQ_FIFO, PicoAudioInputContext::_adc_irq_handler);    
    // Enable ADC->INT
    adc_irq_set_enabled(true);
    // Enable the ADC INT
    irq_set_enabled(ADC_IRQ_FIFO, true);
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

void PicoAudioInputContext::setADCEnabled(bool en) { 

    _adcEnabled = en; 

    // Turn on/off the ADC to minimize interrupt activity when not 
    // actually transmitting.
    adc_run(_adcEnabled);
}

void __not_in_flash_func(PicoAudioInputContext::_interruptHandler)() {

    // Capture time and reset skew tracker
    uint32_t t = _perfTimer.elapsedUs();
    _perfTimer.reset();
    _maxSkew = std::max((uint32_t)_maxSkew, (uint32_t)std::abs(125 - (int32_t)t));

    // We clear one sample every interrupt
    if (adc_fifo_is_empty()) {
        panic("ADC empty");
    }

    // This will be a number from 0->4095 (12 bits).  
    int16_t rawSample = adc_fifo_get();
    // Center to give a number from -2048->2047 (12 bits)
    int16_t sample = rawSample - 2048;
    // Adjust center
    sample += _dcBias;
    // Shift up to form 16-bit PCM.
    int32_t sample32 = sample;
    sample32 <<= 4;
    // Saturate
    if (sample32 > 32767) {
        sample32 = 32767;
    } else if (sample < -32768) {
        sample32 = -32768;
    }

    uint32_t slot = _audioInBufWriteCount.get() & _audioInBufDepthMask;
    _audioInBuf[slot][_audioInBufWritePtr++] = (int16_t)sample32;

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
                // THIS IS AN AUDIO GLITCH!
                _audioInBufOverflow++;
            }
        } 
        else {
            panic("Sanity check 1");
        }
    }

    // Optionally call out to let someone do something on the same sample
    // clock.
    if (_sampleCb) {
        _sampleCb(_sampleCbData);
    }    

    t = _perfTimer.elapsedUs();
    _maxLen = std::max((uint32_t)_maxLen, t);
}    

bool PicoAudioInputContext::run() {

    bool activity = false;

    // Check to see if we have data availble 
    if (_audioInBufWriteCount.get() > _audioInBufReadCount.get()) {

        activity = true;

        uint32_t slot = _audioInBufReadCount.get() & _audioInBufDepthMask;

        // Give the audio to the analyzer, if enabled
        if (_analyzer) {
            _analyzer->play(_audioInBuf[slot], _audioFrameSize * _audioFrameBlockFactor);
        }

        // Pull down a 4xframe and try to move it into the transmitter
        bool accepted = _sink->play(_audioInBuf[slot], _audioFrameSize * _audioFrameBlockFactor);
        if (accepted) {
            // Move the read pointer forward (we already know that 
            // the write counter is larger than the read counter)
            _audioInBufReadCount.inc();
        }
    }
    else if (_audioInBufWriteCount.get() < _audioInBufReadCount.get()) {
        panic("Sanity check 2");
    }

    return activity;
}

}
