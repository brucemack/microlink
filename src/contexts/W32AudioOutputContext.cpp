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
#include <stdexcept>
#include <iostream>

#include "W32AudioOutputContext.h"

using namespace std;

namespace kc1fsz {

int W32AudioOutputContext::traceLevel = 0;

W32AudioOutputContext::W32AudioOutputContext(uint32_t frameSize, 
    uint32_t sampleRate, int16_t* audioArea, int16_t* silenceArea)
:   AudioOutputContext(frameSize, sampleRate),
    _audioData(audioArea),
    _silenceData(silenceArea) {

    MMRESULT result;

    // Specify audio parameters
    WAVEFORMATEX pFormat;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = 1;
    pFormat.nSamplesPerSec = sampleRate;
    pFormat.nAvgBytesPerSec = sampleRate * 2;
    pFormat.nBlockAlign = 2;
    pFormat.wBitsPerSample = 16;
    pFormat.cbSize = 0;

    // Create an event that will be fired by the Wave system when it is 
    // ready to receive some more data.
    _event = CreateEvent( 
        NULL,               // default security attributes
        FALSE,              // TRUE=manual-reset event, FALSE=auto-reset
        FALSE,              // initial state is non-signaled
        TEXT("Done")        // object name
    ); 
    if (_event == 0) {
        throw std::runtime_error("Unable to get Windows event");
    }

    // Open the output channel - using the default output device (WAVE_MAPPER)
    result = waveOutOpen(&_waveOut, WAVE_MAPPER, &pFormat, (DWORD_PTR)_event, 0L, 
        (WAVE_FORMAT_DIRECT | CALLBACK_EVENT));
    if (result) {
        throw std::runtime_error("Unable to open audio output");
    }

    // Set up and prepare buffers/headers for audio output.  We use the buffers
    // in alternating sequence. 
    for (uint32_t b = 0; b < _audioQueueSize; b++) {
        // Calculate the start of the frame
        int16_t* buf = _audioData + (b * _frameSize);
        // Clear
        for (uint32_t i = 0; i < _frameSize; i++) {
            buf[i] = 0;
        }
        _audioHdr[b].lpData = (LPSTR)(buf);
        _audioHdr[b].dwBufferLength = _frameSize * 2;
        _audioHdr[b].dwBytesRecorded = 0;
        _audioHdr[b].dwUser = 0L;
        _audioHdr[b].dwFlags = 0L;
        _audioHdr[b].dwLoops = 0L;
        result = waveOutPrepareHeader(_waveOut, &(_audioHdr[b]), sizeof(WAVEHDR));    
        if (result) {
            throw std::runtime_error("Unable to prepare audio buffer");
        }
    }

    _audioQueuePtr = 0;
    _audioQueueUsed = 0;

    // Set up and prepare buffers/headers for silence output.  We use the buffers
    // in alternating sequence. 
    for (uint32_t b = 0; b < 2; b++) {
        // Calculate the start of the frame
        int16_t* buf = _silenceData + (b * _frameSize);
        // Clear the buffer
        // TODO: GENERATE COMFORT NOISE
        for (uint32_t i = 0; i < _frameSize; i++) {
            buf[i] = 0;
        }
        _silenceHdr[b].lpData = (LPSTR)(buf);
        _silenceHdr[b].dwBufferLength = _frameSize * 2;
        _silenceHdr[b].dwBytesRecorded = 0;
        _silenceHdr[b].dwUser = 0L;
        _silenceHdr[b].dwFlags = 0L;
        _silenceHdr[b].dwLoops = 0L;
        result = waveOutPrepareHeader(_waveOut, &(_silenceHdr[b]), sizeof(WAVEHDR));    
        if (result) {
            throw std::runtime_error("Unable to prepare audio buffer");
        }
    }

    _silenceQueuePtr = 0;
    _inSilence = true;

    // Start sending silence
    waveOutWrite(_waveOut, &(_silenceHdr[_silenceQueuePtr]), sizeof(WAVEHDR));
}

W32AudioOutputContext::~W32AudioOutputContext() {
    waveOutClose(_waveOut);
    CloseHandle(_event);
}

void W32AudioOutputContext::run() {

    // Check the status of the event (and un-signal it atomically if it is set)
    // Timeout is zero
    DWORD r = ::WaitForSingleObject(_event, 0);
    if (r == 0) {

        // Figure out whether it's time to switch between silence and audio
        if (_inSilence) {
            if (_audioQueueUsed > (_audioQueueSize / 2)) {
                //cout << "Switching out of silence" << endl;
                _inSilence = false;
            }
        } else {
            if (_audioQueueUsed == 0) {
                //cout << "Switching into silence" << endl;
                _inSilence = true;
            }
        }
        
        // Send a frame to the audio player, depending on whether we are in 
        // silence or not.
        if (_inSilence) {
            _silenceQueuePtr = (_silenceQueuePtr + 1) % 2;
            waveOutWrite(_waveOut, &(_silenceHdr[_silenceQueuePtr]), sizeof(WAVEHDR));
        } else {
            // Clear out what we just finished with
            int16_t* buf = _audioData + (_audioQueuePtr * _frameSize);
            for (uint32_t i = 0; i < _frameSize; i++) {
                *buf = 0;
            }
            // Feed the next frame
            _audioQueuePtr = (_audioQueuePtr + 1) % _audioQueueSize;
            _audioQueueUsed--;
            waveOutWrite(_waveOut, &(_audioHdr[_audioQueuePtr]), sizeof(WAVEHDR));
        }
    }
}

bool W32AudioOutputContext::play(const int16_t* frame) {

    if (traceLevel >= 1) {
        cout << "(Audio)" << endl;
    }

    // Only allow a write if there are open frames in the queue
    if (_audioQueueUsed < _audioQueueSize) {
        // Calculate where to write the next frame based on the write 
        // pointer and the size of the queue
        int nextBuffer = (_audioQueuePtr + _audioQueueUsed) % _audioQueueSize;
        int16_t* b = _audioData + (nextBuffer * _frameSize);
        for (uint32_t i = 0; i < _frameSize; i++) {
            b[i] = frame[i];
        }
        _audioQueueUsed++;
        return true;
    } else {
        return false;
    }
}

}
