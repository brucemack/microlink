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
#include "machines/WelcomeMachine.h"

#include "TestUserInfo.h"
#include "TestAudioInputContext.h"
#include "Synth.h"

// ===============
// LEFT SIDE PINS 
// ===============

// Serial connection to ESP32
#define UART_TX_PIN 0
#define UART_RX_PIN 1
// Physical pin 11. Ouptut to hard reset on ESP32.
// (Can be on left)
#define ESP_EN_PIN (8)

// Physical pin 10. Output to drive an LED indicating keyed status
#define KEY_LED_PIN (7)
// Physical pin 15. This is an output to drive an LED indicating
// that we are in a QSO. 
#define QSO_LED_PIN (11)
// Physical pin 9.  Input from physical PTT button.
#define PTT_PIN (6)

// ===============
// RIGHT SIDE PINS 
// ===============

#define LED_PIN (25)
// Input from analog section
#define ADC0_PIN (26)
// Physical pin 12.  This is an output (active high) used to key 
// the rig's transmitter. Typically drives an optocoupler to
// get the pull-to-ground needed by the rig.
#define RIG_KEY_PIN (9)
// Physical pin 14. This is an input (active high) used to detect
// receive carrier from the rig. 
#define RIG_COS_PIN (10)
// I2C -> DAC
#define I2C0_SDA (16) // Phy Pin 21: I2C channel 0 - data
#define I2C0_SCL (17) // Phy Pin 22: I2C channel 0 - clock

#define UART_ID uart0
#define U_BAUD_RATE 115200
#define U_DATA_BITS 8
#define U_STOP_BITS 1
#define U_PARITY UART_PARITY_NONE

#define PTT_DEBOUNCE_INTERVAL_MS (250)
#define RIG_COS_DEBOUNCE_INTERVAL_MS (500)

#define TX_TIMEOUT_MS (90 * 1000)
#define TX_LOCKOUT_MS (30 * 1000)

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

    gpio_init(RIG_COS_PIN);
    gpio_set_dir(RIG_COS_PIN, GPIO_IN);
    gpio_pull_up(RIG_COS_PIN);

    // Key/indicator LED
    gpio_init(KEY_LED_PIN);
    gpio_set_dir(KEY_LED_PIN, GPIO_OUT);
    gpio_put(KEY_LED_PIN, 0);

    // QSO indicator LED
    gpio_init(QSO_LED_PIN);
    gpio_set_dir(QSO_LED_PIN, GPIO_OUT);
    gpio_put(QSO_LED_PIN, 0);

    // ESP EN
    gpio_init(ESP_EN_PIN);
    gpio_set_dir(ESP_EN_PIN, GPIO_OUT);
    gpio_put(ESP_EN_PIN, 1);

    // Rig key
    gpio_init(RIG_KEY_PIN);
    gpio_set_dir(RIG_KEY_PIN, GPIO_OUT);
    gpio_put(RIG_KEY_PIN, 0);
       
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
    //gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    //gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    //gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    //gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    //i2c_set_baudrate(i2c_default, 400000 * 4);
    i2c_set_baudrate(i2c_default, 400000);

    // ADC/audio in setup
    PicoAudioInputContext::setup();

    // Hello indicator
    for (int i = 0; i < 4; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }

    cout << "===== MicroLink Link Test 1p ============" << endl;
    cout << "Copyright (C) 2024 Bruce MacKinnon KC1FSZ" << endl;

    PicoUartChannel::traceLevel = 0;
    ESP32CommContext::traceLevel = 0;

    LinkRootMachine::traceLevel = 0;
    LogonMachine::traceLevel = 0;
    QSOAcceptMachine::traceLevel = 0;
    ValidationMachine::traceLevel = 0;
    WelcomeMachine::traceLevel = 1;
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

    ESP32CommContext ctx(&channel, ESP_EN_PIN);

    // Do a flush of any garbage on the serial line before we start 
    // protocol processing.
    ctx.flush(250);

    TestUserInfo info;
    // NOTE: Audio is encoded and decoded in 4-frame chunks.
    I2CAudioOutputContext audioOutContext(audioFrameSize * 4, 8000, 
        audioBufDepthLog2, audioBuf, &info);
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

    bool pttState = false;
    uint32_t lastPttTransition = 0;
    bool lastRigCos = false;
    bool rigCosState = false;
    uint32_t lastRigCosTransition = 0;
    bool rigKeyState = false;
    uint32_t lastRigKeyTransitionTime = 0;
    uint32_t rigKeyLockoutTime = 0;
    uint32_t rigKeyLockoutCount = 0;

    // Start the state machine
    rm.start();

    // Here is the main event loop
    while (true) {

        cycleTimer.reset();

        // ----- External Controls ------------------------------------------

        bool ptt = !gpio_get(PTT_PIN);
        // Simple de-bounce
        if (ptt != pttState && 
            time_ms() > (lastPttTransition + PTT_DEBOUNCE_INTERVAL_MS)) {
            lastPttTransition = time_ms();
            pttState = ptt;
            audioInContext.setPtt(pttState);
        }

        bool rigCos = gpio_get(RIG_COS_PIN);
        // Look for activity on the line (not debounced)
        if (rigCos != lastRigCos) {
            lastRigCosTransition = time_ms();
        }
        lastRigCos = rigCos;

        // If the carrier is currently not detected
        if (rigCosState == false) {
            // The LO->HI transition is taken immediately
            if (rigCos) {
                if (info.getSquelch() || 
                    info.getMsSinceLastSquelchClose() < 500) {
                } 
                else {
                    cout << "COS on" << endl;
                    rigCosState = true;
                    if (rm.isInQSO()) {
                        audioInContext.setPtt(rigCosState);
                    }
                }
            }
        } 
        // If the carrier is currently active
        else {
            // The HI->LO transition is fully debounced
            if (!rigCos && 
                (time_ms() - lastRigCosTransition) > RIG_COS_DEBOUNCE_INTERVAL_MS) {
                if (!rigCos) {
                    cout << "COS off" << endl;
                }
                rigCosState = false;
                audioInContext.setPtt(rigCosState);
            }
        }

        // ----- Indicator Lights --------------------------------------------

        if (audioInContext.getPtt() || 
            (info.getSquelch() && (time_ms() % 1024) > 512)) {
            gpio_put(KEY_LED_PIN, 1);
        } 
        else {
            gpio_put(KEY_LED_PIN, 0);
        }

        gpio_put(QSO_LED_PIN, rm.isInQSO() ? 1 : 0);

        // ----- Rig Key Management -----------------------------------------
        // Rig key when audio is coming in, but enforce limits to prevent
        // the key from being stuck open for long periods.

        if (!rigKeyState) {
            if (info.getSquelch() && 
                time_ms() > (rigKeyLockoutTime + TX_LOCKOUT_MS)) {
                info.setStatus("Keying rig");
                rigKeyState = true;
                lastRigKeyTransitionTime = time_ms();
            }
        }
        else {
            // Check for normal unkey
            if (!info.getSquelch()) {
                info.setStatus("Unkeying rig");
                rigKeyState = false;
                lastRigKeyTransitionTime = time_ms();
            }
            // Look for timeout case
            else if (time_ms() > lastRigKeyTransitionTime + TX_TIMEOUT_MS) {
                info.setStatus("TX lockout triggered");
                rigKeyState = false;
                lastRigKeyTransitionTime = time_ms();
                rigKeyLockoutTime = time_ms();
                rigKeyLockoutCount++;
            }
        }

        gpio_put(RIG_KEY_PIN, rigKeyState ? 1 : 0);

        // ----- Serial Commands ---------------------------------------------
        
        int c = getchar_timeout_us(0);
        if (c > 0) {
            if (c == 's') {
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
                cout << "TX lockout count  : " << rigKeyLockoutCount << endl;

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
