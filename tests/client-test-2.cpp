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
 * 
 * This test case is targeted at the Windows platform. This is a basic EL client.
 */
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/win32/Win32PerfTimer.h"

#include "machines/RootMachine.h"
#include "contexts/SocketContext.h"
#include "contexts/W32AudioOutputContext.h"

#include "TestUserInfo.h"
#include "TestAudioInputContext.h"

using namespace std;
using namespace kc1fsz;

// The size of one EchoLink RTP packet (after decoding)
static const int audioFrameSize = 160 * 4;
static const int audioFrameCount = 16;
// Double-buffer
static int16_t audioFrameOut[audioFrameCount * audioFrameSize];
// Double-buffer
static int16_t silenceFrameOut[2 * audioFrameSize];

int main(int, const char**) {

    cout << "===== MicroLink Test 2 ==================" << endl;
    cout << "Copyright (C) 2024 Bruce MacKinnon KC1FSZ" << endl;

    RootMachine::traceLevel = 0;
    LogonMachine::traceLevel = 0;
    LookupMachine2::traceLevel = 0;
    QSOConnectMachine::traceLevel = 1;
    QSOFlowMachine::traceLevel = 1;
    SocketContext::traceLevel = 0;
    W32AudioOutputContext::traceLevel = 1;

    if (getenv("EL_CALLSIGN") == 0 || getenv("EL_PASSWORD") == 0 ||
        getenv("EL_FULLNAME") == 0 || getenv("EL_LOCATION") == 0) {
        cout << "EL_CALLSIGN, EL_PASSWORD, EL_FULLNAME, EL_LOCATION must be set." << endl;
        return -1;
    }

    SocketContext context;
    TestUserInfo info;
    W32AudioOutputContext audioOutContext(audioFrameSize, 8000, audioFrameOut, silenceFrameOut);
    TestAudioInputContext audioInContext(audioFrameSize, 8000);

    RootMachine rm(&context, &info, &audioOutContext);
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setServerPort(5200);
    rm.setCallSign(CallSign(getenv("EL_CALLSIGN")));
    rm.setPassword(FixedString(getenv("EL_PASSWORD")));
    rm.setFullName(FixedString(getenv("EL_FULLNAME")));
    rm.setLocation(FixedString(getenv("EL_LOCATION")));
    rm.setTargetCallSign(CallSign("*ECHOTEST*"));

    context.setEventProcessor(&rm);
    // Send input audio into station transmit port
    audioInContext.setSink(&rm);

    TickEvent tickEv;
    uint32_t lastAudioTickMs = 0;

    Win32PerfTimer socketTimer;
    Win32PerfTimer audioTimer;
    uint32_t longestSocketUs = 0;
    uint32_t longestAudioUs = 0;

    // Here is the main event loop
    uint32_t start = time_ms();
    uint32_t cycle = 0;

    // Make stdin non-blocking 
    fcntl(0, F_SETFL, O_NONBLOCK); 

    while (true) {

        int c = getchar();
        if (c == 'q') {
            cout << "Leaving event loop" << endl;
            break;
        }
        else if (c == 't') {
            cout << "Transmit tone" << endl;  
            // Short burst of tone
            audioInContext.sendTone(1000, 500);
        } 
        else if (c == 's') {
            cout << "Starting" << endl;
            rm.start();
        }

        // Poll the audio input system at full speed
        audioInContext.poll();

        // Poll the audio output system at full speed
        audioTimer.reset();
        bool audioActivity = audioOutContext.poll();
        uint32_t ela = audioTimer.elapsedUs();
        if (ela > longestAudioUs) {
            longestAudioUs = ela;
            cout << "Longest Audio " << longestAudioUs << endl;
        }

        // Poll the communications system
        socketTimer.reset();
        bool commActivity = context.poll();
        ela = socketTimer.elapsedUs();
        if (ela > longestSocketUs) {
            longestSocketUs = ela;
            cout << "Longest Socket " << longestSocketUs << endl;
        }

        bool activity = audioActivity || commActivity;

        // Generate the audio clock every 80ms (160*4 samples)
        uint32_t now = time_ms();
        if (now - lastAudioTickMs >= 80) {
            lastAudioTickMs = now;
            rm.processEvent(&tickEv);
        }

        // This is here only to avoid high-CPU loops
        // Tell the context to move forward
        //uint32_t t0 = time_ms();
        //std::this_thread::sleep_for(std::chrono::microseconds(500));
        std::this_thread::yield();
 
        if (!activity)
            Sleep(5);

        cycle++;
    }
}
