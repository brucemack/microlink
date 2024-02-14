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

openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program link-test-1p.elf verify reset exit"
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

#include "contexts/ESP32CommContext.h"
#include "contexts/I2CAudioOutputContext.h"
#include "contexts/PicoAudioInputContext.h"

#include "machines/LinkRootMachine.h"
#include "machines/QSOFlowMachine.h"
#include "machines/QSOAcceptMachine.h"

#include "TestUserInfo.h"
#include "TestAudioInputContext.h"

#define LED_PIN (25)
// Physical pin 9
#define PTT_PIN (6)
// Physical pin 10
#define KEY_LED_PIN (7)
// Physical pin 11
#define ESP_EN_PIN (8)

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

int main(int, const char**) {

    // Seup PICO
    stdio_init_all();

    // On-board LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // PTT switch
    gpio_init(PTT_PIN);
    gpio_set_dir(PTT_PIN, GPIO_IN);
    gpio_pull_up(PTT_PIN);

    // Key LED
    gpio_init(KEY_LED_PIN);
    gpio_set_dir(KEY_LED_PIN, GPIO_OUT);
    gpio_put(KEY_LED_PIN, 0);

    // ESP EN
    gpio_init(ESP_EN_PIN);
    gpio_set_dir(ESP_EN_PIN, GPIO_OUT);
    gpio_put(ESP_EN_PIN, 1);
       
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
    //i2c_set_baudrate(i2c_default, 400000 * 4);
    i2c_set_baudrate(i2c_default, 400000);

    // ADC/audio in setup
    PicoAudioInputContext::setup();

    // Reset ESP
    gpio_put(ESP_EN_PIN, 1);
    sleep_ms(500);
    gpio_put(ESP_EN_PIN, 0);
    sleep_ms(500);
    gpio_put(ESP_EN_PIN, 1);

    // Hello indicator
    for (int i = 0; i < 4; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }

    cout << "===== MicroLink Link Test 1p ============" << endl;
    cout << "Copyright (C) 2024 Bruce MacKinnon KC1FSZ" << endl;

    PicoUartChannel::traceLevel = 1;
    ESP32CommContext::traceLevel = 1;

    LinkRootMachine::traceLevel = 1;
    LogonMachine::traceLevel = 1;
    QSOAcceptMachine::traceLevel = 1;
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

    LinkRootMachine rm(&ctx, &info, &audioOutContext);

    // Cross-connects
    info.setAudioOut(&audioOutContext);
    audioInContext.setSink(&rm);
    ctx.setEventProcessor(&rm);

    // TODO: Move configuration out 
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setServerPort(5200);
    rm.setCallSign(CallSign("W1TKZ-L"));
    rm.setPassword(FixedString("xxx"));
    rm.setFullName(FixedString("Wellesley Amateur Radio Society"));
    rm.setLocation(FixedString("Wellesley, MA USA"));

    const uint32_t taskCount = 4;
    Runnable* tasks[taskCount] = {
        &audioOutContext, &audioInContext, 
        &ctx, &rm
    };
    uint32_t maxTaskTime[taskCount] = { 
        0, 0, 
        0, 0 };

    PicoPerfTimer cycleTimer;
    uint32_t longestCycleUs = 0;
    uint32_t longCycleCounter = 0;
    PicoPerfTimer taskTimer;

    // Here is the main event loop
    bool pttState = false;
    uint32_t lastPttTransition = 0;

    while (true) {

        cycleTimer.reset();

        // Physical controls
        bool ptt = !gpio_get(PTT_PIN);
        // Simple de-bounce
        if (ptt != pttState && time_ms() > (lastPttTransition + 250)) {
            lastPttTransition = time_ms();
            pttState = ptt;
            audioInContext.setPtt(pttState);
        }

        // Keyboard input
        int c = getchar_timeout_us(0);
        if (c > 0) {
            if (c == 's') {
                //if (!rm.isInQSO()) {
                    cout << endl << "Starting" << endl;
                    rm.start();
                //}
            }
            else if (c == 'x') {
                cout << endl << "Stoppng" << endl;
                rm.requestCleanStop();
            }
            else if (c == 'q') {
                break;
            } 
            else if (c == ' ') {
                audioInContext.setPtt(!audioInContext.getPtt());
                cout << endl << "Keyed: " << audioInContext.getPtt() << endl;
            }
            else if (c == 'e') {
                cout << endl << "ESP32 Test: " <<  ctx.test() << endl;
            }
            else if (c == 'z') {
                audioOutContext.tone(800, 500);
            }
            else if (c == 'i') {
                cout << endl;
                cout << "Diagnostics" << endl;
                cout << "Audio In Overflow : " << audioInContext.getOverflowCount() << endl;
                cout << "Audio In Avg      : " << audioInContext.getAverage() << endl;
                cout << "Audio In Avg%     : " << (100 * audioInContext.getAverage()) / 32767 << endl;
                cout << "Audio In Max      : " << audioInContext.getMax() << endl;
                cout << "Audio In Max%     : " << (100 * audioInContext.getMax()) / 32767 << endl;
                cout << "Audio In Clips    : " << audioInContext.getClips() << endl;
                cout << "Audio Gain        : " << audioInContext.getGain() << endl;
                cout << "UART RX COUNT     : " << channel.getBytesReceived() << endl;
                cout << "UART RX LOST      : " << channel.getReadBytesLost() << endl;
                cout << "UART TX COUNT     : " << channel.getBytesSent() << endl;
                cout << "Long Cycles       : " << longCycleCounter << endl;

                for (uint32_t t = 0; t < taskCount; t++) {
                    cout << "Task " << t << " max " << maxTaskTime[t] << endl;
                }
            } 
            else if (c == '=') {
                audioInContext.setGain(audioInContext.getGain() + 1);
                cout << "Gain is " << audioInContext.getGain() << endl;
            }
            else if (c == '-') {
                audioInContext.setGain(audioInContext.getGain() - 1);
                cout << "Gain is " << audioInContext.getGain() << endl;
            }
            else if (c == 'c') {
                cout << "Clear Stats" << endl;
                for (uint32_t t = 0; t < taskCount; t++) {
                    maxTaskTime[t] = 0;
                }
            }
            else {
                cout << (char)c;
                cout.flush();
            }
        }

        // Indicator lights
        if (audioInContext.getPtt() || 
            (info.getSquelch() && (time_ms() % 1024) > 512)) {
            gpio_put(KEY_LED_PIN, 1);
        } 
        else {
            gpio_put(KEY_LED_PIN, 0);
        }

        // Run the tasks, keeping track of the time for each
        for (uint32_t t = 0; t < taskCount; t++) {
            taskTimer.reset();
            tasks[t]->run();
            maxTaskTime[t] = std::max(maxTaskTime[t], taskTimer.elapsedUs());
        }

        uint32_t ela = cycleTimer.elapsedUs();
        if (ela > longestCycleUs) {
            longestCycleUs = ela;
            cout << "Longest Cycle (us) " << longestCycleUs << endl;
        }
        if (ela > 125) {
            longCycleCounter++;
        }
    }

    cout << "Left event loop" << endl;

    while (true) {        
    }
}
