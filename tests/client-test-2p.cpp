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
 * This test runs on the RP2040 hardware and provides a fairly comprehensive test
 * of connection, logon, and receipt of audio packets from the *ECHOTEST* station.
 */

/*
Launch command:

openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program client-test-2p.elf verify reset exit"
*/
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <cmath>
#include <atomic>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/rp2040/PicoUartChannel.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"

#include "machines/RootMachine.h"
#include "contexts/ESP32CommContext.h"
#include "contexts/I2CAudioOutputContext.h"
#include "contexts/PicoAudioInputContext.h"

#include "TestUserInfo.h"
#include "TestAudioInputContext.h"

#define LED_PIN (25)

#define UART_ID uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define U_BAUD_RATE 115200
#define U_DATA_BITS 8
#define U_STOP_BITS 1
#define U_PARITY UART_PARITY_NONE

#define I2C0_SDA (4) // Phy Pin 6: I2C channel 0 - data
#define I2C0_SCL (5) // Phy Pin 7: I2C channel 0 - clock

using namespace std;
using namespace kc1fsz;

// The size of one EchoLink audio frame (after decoding)
static const int audioFrameSize = 160;
// The number of audio frames packed into an RTP packet
static const uint32_t audioFrameBlockFactor = 4;

// Provide buffer for about a second of audio.  We round up to 16 frames worth.
static const uint32_t audioBufDepth = 16;
static const uint32_t audioBufDepthLog2 = 4;
static int16_t audioBuf[audioFrameSize * 4 * audioBufDepth];

// TODO: This has some audio quality problems
static void testTone(AudioOutputContext& ctx) {

    // Make a 1kHz tone at the right sample rate
    int16_t buf[audioFrameSize * 4];
    float omega = (2.0 * 3.1415926) * (1000.0 / 8000.0);
    float phi = 0;
    for (uint32_t i = 0; i < audioFrameSize * 4; i++) {
        float a = std::cos(phi);
        phi += omega;
        buf[i] = 32766.0 * a;
    }

    // Mini blocking event loop (2 seconds)
    PicoPollTimer timer;
    PicoPerfTimer timer2;
    timer2.reset();
    timer.setIntervalUs(125 * 160 * 4);
    uint32_t frameCount = 0;
    uint32_t actCount = 0;
    uint32_t longestPoll = 0;

    ctx.reset();

    while (frameCount < 25) {
        // Keep the audio going
        timer2.reset();
        if (ctx.poll()) {
            actCount++;
        }
        if (timer2.elapsedUs() > longestPoll) {
            longestPoll = timer2.elapsedUs();
        }
        // Figure out if it's time to feed more
        if (timer.poll()) {
            ctx.play(buf);
            frameCount++;
        }
    }
    cout << "Longest Poll   : " << longestPoll << endl;
    cout << "Activity Count : " << actCount << endl;
    cout << "Sync Errors    : " << ctx.getSyncErrorCount() << endl;
}

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

    // Setup I2C
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    i2c_set_baudrate(i2c_default, 400000 * 4);

    // ADC/audio in setup
    PicoAudioInputContext::setup();

    // Hello indicator
    gpio_put(LED_PIN, 1);
    sleep_ms(1000);
    gpio_put(LED_PIN, 0);
    sleep_ms(1000);

    cout << "===== MicroLink Test 2p =================" << endl;
    cout << "Copyright (C) 2024 Bruce MacKinnon KC1FSZ" << endl;

    PicoUartChannel::traceLevel = 0;
    ESP32CommContext::traceLevel = 1;

    RootMachine::traceLevel = 0;
    LogonMachine::traceLevel = 0;
    LogonMachine::traceLevel = 0;
    LookupMachine2::traceLevel = 0;
    QSOConnectMachine::traceLevel = 0;
    QSOFlowMachine::traceLevel = 0;

    // Sertup UART and timer
    const uint32_t readBufferSize = 256;
    uint8_t readBuffer[readBufferSize];
    const uint32_t writeBufferSize = 256;
    uint8_t writeBuffer[writeBufferSize];

    PicoUartChannel channel(UART_ID, 
        readBuffer, readBufferSize, writeBuffer, writeBufferSize);

    PicoPollTimer timer;
    timer.setIntervalUs(1000 * 5000);

    ESP32CommContext ctx(&channel);

    // Do a flush of any garbage on the serial line before we start 
    // protocol processing.
    ctx.flush(250);

    TestUserInfo info;
    // NOTE: Audio is encoded and decoded in 4-frame chunks.
    I2CAudioOutputContext audioOutContext(audioFrameSize * 4, 8000, 
        audioBufDepthLog2, audioBuf);
    PicoAudioInputContext audioInContext;
    //TestAudioInputContext audioInContext(audioFrameSize * 4, 8000);

    RootMachine rm(&ctx, &info, &audioOutContext);
    audioInContext.setSink(&rm);

    // TODO: Move configuration out 
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setServerPort(5200);
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString("xxx"));
    rm.setFullName(FixedString("Bruce R. MacKinnon"));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    rm.setTargetCallSign(CallSign("*ECHOTEST*"));

    ctx.setEventProcessor(&rm);

    TickEvent tickEv;

    PicoPerfTimer socketTimer;
    uint32_t longestSocketUs = 0;

    // Here is the main event loop
    uint32_t cycle = 0;

    while (true) {

        int c = getchar_timeout_us(0);
        if (c > 0) {
            if (c == 's') {
                cout << endl << "Starting" << endl;
                rm.start();
            }
            else if (c == 'q') {
                break;
            } 
            else if (c == 't') {
                cout << endl << "TX test" << endl;
                // Short burst of tone
                //audioInContext.sendTone(1000, 2000);
            } 
            else if (c == ' ') {
                audioInContext.setPtt(!audioInContext.getPtt());
                cout << endl << "Keyed: " << audioInContext.getPtt() << endl;
            }
            else if (c == 'e') {
                cout << endl << "ESP32 Test: " <<  ctx.test() << endl;
            }
            else if (c == 'z') {
                testTone(audioOutContext);
            }
            else if (c == 'i') {
                cout << endl;
                cout << "Diagnostics" << endl;
                cout << "Audio In Overflow : " << audioInContext.getOverflowCount() << endl;
                cout << "UART TX IRQ       : " << channel.getIsrCountWrite() << endl;
            } 
            else {
                cout << (char)c;
                cout.flush();
            }
        }

        if (rm.isDone()) {
            break;
        }

        // Poll the audio system
        audioOutContext.poll();
        audioInContext.poll();

        // Poll the communications system and pass any inbound bytes
        // over to the communications context.
        socketTimer.reset();
        ctx.poll();
        uint32_t ela = socketTimer.elapsedUs();
        if (ela > longestSocketUs) {
            longestSocketUs = ela;
            cout << "Longest Socket (us) " << longestSocketUs << endl;
        }

        // Run continuously
        rm.processEvent(&tickEv);

        /*
        // Generate the one second tick (needed for timeouts, etc)
        uint32_t now = time_ms();
        if (now - lastAudioTickMs >= 1000) {
            lastAudioTickMs = now;
            rm.processEvent(&tickEv);
        }
        */

        // Used to show that we are still alive
        cycle++;
        if (cycle % 10000000 == 0) {
            cout << cycle << endl;
        }
    }

    cout << "Left event loop" << endl;

    while (true) {        
    }
}
