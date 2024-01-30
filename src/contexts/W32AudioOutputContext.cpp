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

W32AudioOutputContext::W32AudioOutputContext(uint32_t frameSize, 
    uint32_t sampleRate, int16_t* bufferArea)
:   AudioOutputContext(frameSize, sampleRate),
    _waveData(bufferArea),
    _frameCount(0) {

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

    // Set up and prepare buffers/headers for output.  We use the buffers
    // in alternating sequence. 
    for (int b = 0; b < 2; b++) {
        // Calculate the start of the frame
        int16_t* buf = _waveData + (b * _frameSize);
        // Clear
        for (uint32_t i = 0; i < _frameSize; i++) {
            buf[i] = 0;
        }
        _waveHdr[b].lpData = (LPSTR)(buf);
        _waveHdr[b].dwBufferLength = _frameSize * 2;
        _waveHdr[b].dwBytesRecorded = 0;
        _waveHdr[b].dwUser = 0L;
        _waveHdr[b].dwFlags = 0L;
        _waveHdr[b].dwLoops = 0L;
        result = waveOutPrepareHeader(_waveOut, &(_waveHdr[b]), sizeof(WAVEHDR));    
        if (result) {
            throw std::runtime_error("Unable to prepare audio buffer");
        }
    }

    _frameCount  = 0;
    int nextBuffer = _frameCount % 2;
    _frameCountOnLastWrite = 0;
    
    // Get things going by launching the first buffer
    result = waveOutWrite(_waveOut, &(_waveHdr[nextBuffer]), sizeof(WAVEHDR));
    if (result) {
        throw std::runtime_error("Unable to write audio");
    }
}

W32AudioOutputContext::~W32AudioOutputContext() {
    waveOutClose(_waveOut);
    CloseHandle(_event);
}

bool W32AudioOutputContext::poll() {

    bool anythingHappened = false;

    // Check the status of the event (and un-signal it atomically if it is set)
    // Timeout is zero
    DWORD r = ::WaitForSingleObject(_event, 0);
    if (r == 0) {

        anythingHappened = true;

        int currentBuffer = _frameCount % 2;
        int nextBuffer = (_frameCount + 1) % 2;
        _frameCount++;

        // Clear the current buffer so that we get silence by default
        // (in the underflow situation)
        int16_t* b = _waveData + (currentBuffer * _frameSize);
        for (uint32_t i = 0; i < _frameSize; i++) {
            *b = 0;
        }

        // Serve up the "other" buffer in alternating sequence
        waveOutWrite(_waveOut, &(_waveHdr[nextBuffer]), sizeof(WAVEHDR));
    }

    return anythingHappened;
}

void W32AudioOutputContext::play(int16_t* frame) {

    int delta = (int)_frameCount - (int)_frameCountOnLastWrite;
    cout << "Play delta " << delta << endl;    

    int nextBuffer = (_frameCount + 1) % 2;
    int16_t* b = _waveData + (nextBuffer * _frameSize);
    for (uint32_t i = 0; i < _frameSize; i++) {
        b[i] = frame[i];
    }
    _frameCountOnLastWrite = _frameCount;
}

}
