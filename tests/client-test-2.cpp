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

    if (getenv("EL_PASSWORD") == 0) {
        cout << "EL_PASSWORD must be set." << endl;
        return -1;
    }

    SocketContext context;
    TestUserInfo info;
    W32AudioOutputContext audioOutContext(audioFrameSize, 8000, audioFrameOut, silenceFrameOut);

    RootMachine rm(&context, &info, &audioOutContext);
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setServerPort(5200);
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString(getenv("EL_PASSWORD")));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    //rm.setTargetCallSign(CallSign("W1TKZ-L"));
    rm.setTargetCallSign(CallSign("*ECHOTEST*"));

    rm.start();

    TickEvent tickEv;
    uint32_t lastAudioTickMs = 0;

    Win32PerfTimer socketTimer;
    Win32PerfTimer audioTimer;
    uint32_t longestSocketUs = 0;
    uint32_t longestAudioUs = 0;

    // Here is the main event loop
    uint32_t start = time_ms();
    uint32_t cycle = 0;

    while ((time_ms() - start) < 30000) {

        // Poll the audio system 
        audioTimer.reset();
        bool audioActivity = audioOutContext.poll();
        uint32_t ela = audioTimer.elapsedUs();
        if (ela > longestAudioUs) {
            longestAudioUs = ela;
            cout << "Longest Audio " << longestAudioUs << endl;
        }
        
        // Poll the communications system
        socketTimer.reset();
        bool commActivity = context.poll(&rm);
        ela = socketTimer.elapsedUs();
        if (ela > longestSocketUs) {
            longestSocketUs = ela;
            cout << "Longest Socket " << longestSocketUs << endl;
        }

        bool activity = audioActivity || commActivity;

        // Generate the audio clock every 20ms (160*4 samples)
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
