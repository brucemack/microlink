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

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/rp2040/PicoUartChannel.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"

#include "machines/RootMachine.h"
#include "contexts/ESP32CommContext.h"
#include "TestAudioOutputContext.h"
#include "TestUserInfo.h"

const uint LED_PIN = 25;

#define UART_ID uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define U_BAUD_RATE 115200
#define U_DATA_BITS 8
#define U_STOP_BITS 1
#define U_PARITY UART_PARITY_NONE

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

    // Seup PICO
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
       
    // UART0 setup
    uart_init(UART_ID, U_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, U_DATA_BITS, U_STOP_BITS, U_PARITY);
    uart_set_fifo_enabled(UART_ID, true);
    uart_set_translate_crlf(UART_ID, false);

    gpio_put(LED_PIN, 1);
    sleep_ms(1000);
    gpio_put(LED_PIN, 0);
    sleep_ms(1000);

    cout << "MicroLink Test 2p" << endl;

    // Sertup UART and timer
    const uint32_t readBufferSize = 256;
    uint8_t readBuffer[readBufferSize];
    const uint32_t writeBufferSize = 256;
    uint8_t writeBuffer[writeBufferSize];

    PicoUartChannel channel(UART_ID, 
        readBuffer, readBufferSize, writeBuffer, writeBufferSize);

    PicoPollTimer timer;
    timer.setIntervalUs(1000 * 5000);

    ESP32CommContext context(&channel);
    TestUserInfo info;
    TestAudioOutputContext audioOutContext(audioFrameSize, 8000);

    RootMachine rm(&context, &info, &audioOutContext);
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString(getenv("EL_PASSWORD")));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    //rm.setTargetCallSign(CallSign("W1TKZ-L"));
    rm.setTargetCallSign(CallSign("*ECHOTEST*"));

    context.setEventProcessor(&rm);

    TickEvent tickEv;
    uint32_t lastAudioTickMs = 0;

    PicoPerfTimer socketTimer;
    PicoPerfTimer audioTimer;
    uint32_t longestSocketUs = 0;
    uint32_t longestAudioUs = 0;

    // Here is the main event loop
    uint32_t start = time_ms();
    uint32_t cycle = 0;

    rm.start();

    while (true) {

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
        bool commActivity = context.poll();
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

        cycle++;
    }
}
