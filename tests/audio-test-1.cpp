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
 * 
 * =================================================================================
 * This file is unit-test code only.  None of this should be use for 
 * real applications!
 * =================================================================================
 */
#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>

//#include "common.h"

using namespace std;
//using namespace kc1fsz;

const int NUMPTS = 8000 * 2;
// Double-buffer
int16_t waveData[2][NUMPTS];

// A simple example of playing an audio tone using alternating buffers.
//
// For API details see:
// https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutopen
//
int main(int, const char**) {

    // Get the number of Digital Audio Out devices in this computer 
    // and display information about all of them.
    unsigned long iNumDevs = waveOutGetNumDevs();
    for (uint32_t i = 0; i < iNumDevs; i++) {
        WAVEOUTCAPS woc;
        if (!waveOutGetDevCaps(i, &woc, sizeof(WAVEOUTCAPS))) {
            printf("Device ID #%u: %s\r\n", i, woc.szPname);
        }
    }

    MMRESULT result;

    // Specify recording parameters
    int sampleRate = 8000;
    WAVEFORMATEX pFormat;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = 1;
    pFormat.nSamplesPerSec = sampleRate;
    pFormat.nAvgBytesPerSec = sampleRate * 2;
    pFormat.nBlockAlign = 2;
    pFormat.wBitsPerSample = 16;
    pFormat.cbSize = 0;

    // Create an event that will be fired
    HANDLE event = CreateEvent( 
        NULL,               // default security attributes
        FALSE,               // TRUE=manual-reset event, FALSE=auto-reset
        FALSE,              // initial state is nonsignaled
        TEXT("Done")        // object name
        ); 
    if (event == 0) {
        cout << "Event failed" << endl;
        return -1;
    }

    // Open the output channel - using the default output device (WAVE_MAPPER)
    HWAVEOUT waveOut;
    result = waveOutOpen(&waveOut, WAVE_MAPPER, &pFormat, (DWORD_PTR)event, 0L, 
        (WAVE_FORMAT_DIRECT | CALLBACK_EVENT));
    if (result) {
        cout << "Open failed" << endl;
        return -1;
    }

    // Set up and prepare buffers/headers for output
    WAVEHDR waveHdr[2];

    waveHdr[0].lpData = (LPSTR)waveData[0];
    waveHdr[0].dwBufferLength = NUMPTS * 2;
    waveHdr[0].dwBytesRecorded = 0;
    waveHdr[0].dwUser = 0L;
    waveHdr[0].dwFlags = 0L;
    waveHdr[0].dwLoops = 0L;
    result = waveOutPrepareHeader(waveOut, &(waveHdr[0]), sizeof(WAVEHDR));    
    if (result) {
        cout << "Prepare 0 failed" << endl;
        return -1;
    }

    waveHdr[1].lpData = (LPSTR)waveData[1];
    waveHdr[1].dwBufferLength = NUMPTS * 2;
    waveHdr[1].dwBytesRecorded = 0;
    waveHdr[1].dwUser = 0L;
    waveHdr[1].dwFlags = 0L;
    waveHdr[1].dwLoops = 0L;
    result = waveOutPrepareHeader(waveOut, &(waveHdr[1]), sizeof(WAVEHDR));    
    if (result) {
        cout << "Prepare 1 failed" << endl;
        return -1;
    }

    // Fill buffers with a consistent tone
    float freqRad = 667.0 * 2.0 * 3.1415926;
    float omega = freqRad / (float)sampleRate;
    float phi = 0;
    for (int b = 0; b < 2; b++) {
        for (int i = 0; i < NUMPTS; i++) {
            waveData[b][i] = 32767.0 * std::cos(phi);
            phi += omega;
        }
    }

    int bufferCount = 0;
    int nextBuffer = bufferCount % 2;

    // Get things going by launching the first buffer
    result = waveOutWrite(waveOut, &(waveHdr[nextBuffer]), sizeof(WAVEHDR));
    if (result) {
        cout << "Write 0 failed" << endl;
        return -1;
    }

    for (int i = 0; i < 40; i++) {

        // Check the status of the event (and un-signal it atomically if it is set)
        DWORD r = ::WaitForSingleObject(event, 0);
        if (r == 0) {

            bufferCount++;
            nextBuffer = bufferCount % 2;

            // Serve up the "other" buffer in alternating sequence
            cout << "Delivering buffer " << nextBuffer << endl;
            result = waveOutWrite(waveOut, &(waveHdr[nextBuffer]), sizeof(WAVEHDR));
            if (result) {
                cout << "Write failed" << endl;
                return -1;
            }
        }

        cout << "Tick " << i << endl;
        Sleep(250);
    }
}
