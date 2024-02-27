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
openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program link-main.elf verify reset exit"
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
#include "hardware/watchdog.h"
#include "hardware/flash.h"

#include "kc1fsz-tools/AudioAnalyzer.h"
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
#include "machines/LookupMachine2.h"
#include "machines/QSOConnectMachine.h"

#include "../tests/TestUserInfo.h"

#include "Synth.h"
#include "RXMonitor.h"

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
// Physical pin 20.  Used for diagnostics/timing checks
#define DIAG_PIN (15)

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

#define RIG_COS_DEBOUNCE_INTERVAL_MS (500)

#define TX_TIMEOUT_MS (90 * 1000)
#define TX_LOCKOUT_MS (30 * 1000)

// This controls the maximum delay before the watchdog
// will reboot the system
#define WATCHDOG_DELAY_MS (2 * 1000)

// The version of the configuration version that we expect 
// to find (must be at least this version)
#define CONFIG_VERSION (0xbab0)

using namespace std;
using namespace kc1fsz;

// Audio rate
static const uint32_t sampleRate = 8000;
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

    // Diag
    gpio_init(DIAG_PIN);
    gpio_set_dir(DIAG_PIN, GPIO_OUT);
    gpio_put(DIAG_PIN, 0);
       
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
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    //i2c_set_baudrate(i2c_default, 400000 * 4);
    i2c_set_baudrate(i2c_default, 400000);

    // Hello indicator
    for (int i = 0; i < 4; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }

    cout << "===== MicroLink Link Station ============" << endl;
    cout << "Copyright (C) 2024 Bruce MacKinnon KC1FSZ" << endl;

    if (watchdog_caused_reboot()) {
        cout << "WATCHDOG REBOOT" << endl;
    } else if (watchdog_enable_caused_reboot()) {
        cout << "WATCHDOG EANBLE REBOOT" << endl;
    } else {
        cout << "Normal reboot" << endl;
    }

    /*
    // TEMPORARY!
    {
        // Write flash
        StationConfig config;
        config.version = 0xbab0;
        strncpy(config.addressingServerHost, "naeast.echolink.org", 32);
        config.addressingServerPort = 5200;
        strncpy(config.callSign, "W1TKZ-L", 32);
        strncpy(config.password, "xxx", 32);
        strncpy(config.fullName, "Wellesley Amateur Radio Society", 32);
        strncpy(config.location, "Wellesley, MA USA FN42", 32);
        uint32_t ints = save_and_disable_interrupts();
        // Must erase a full sector first (4096 bytes)
        flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
        // IMPORTANT: Must be a multiple of 256!
        flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t*)&config, 256);
        restore_interrupts(ints);
    } 
    */   

    // ----- READ CONFIGURATION FROM FLASH ------------------------------------

    HostName addressingServerHost;
    uint32_t addressingServerPort;
    CallSign ourCallSign;
    FixedString ourPassword;
    FixedString ourFullName;
    FixedString ourLocation;

    // The very last sector of flash is used. Compute the memory-mapped address, 
    // remembering to include the offset for RAM
    const uint8_t* addr = (uint8_t*)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
    auto p = (const StationConfig*)addr;
    if (p->version != CONFIG_VERSION) {
        cout << "ERROR: Configuration data is invalid" << endl;
        panic_unsupported();
        return -1;
    } 

    addressingServerHost = HostName(p->addressingServerHost);
    addressingServerPort = p->addressingServerPort;
    ourCallSign = CallSign(p->callSign);
    ourPassword = FixedString(p->password);
    ourFullName = FixedString(p->fullName);
    ourLocation = FixedString(p->location);

    cout << "EL Addressing Server : " << addressingServerHost.c_str() 
        << ":" << addressingServerPort << endl;
    cout << "Identification       : " << ourCallSign.c_str() << "/" 
        << ourFullName.c_str() << "/" << ourLocation.c_str() << endl;

    // ADC/audio in setup
    PicoAudioInputContext::setup();

    PicoUartChannel::traceLevel = 0;
    ESP32CommContext::traceLevel = 2;

    LinkRootMachine::traceLevel = 0;
    LogonMachine::traceLevel = 0;
    QSOAcceptMachine::traceLevel = 0;
    ValidationMachine::traceLevel = 0;
    WelcomeMachine::traceLevel = 0;
    QSOFlowMachine::traceLevel = 0;

    LookupMachine2::traceLevel = 0;
    QSOConnectMachine::traceLevel = 0;

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
    I2CAudioOutputContext audioOutContext(audioFrameSize * 4, sampleRate, 
        audioBufDepthLog2, audioBuf, &info);
    PicoAudioInputContext audioInContext;

    // Connect the input (ADC) timer to the output (DAC)
    audioInContext.setSampleCb(I2CAudioOutputContext::tickISR, &audioOutContext);

    // Analyzers for sound data
    int16_t txAnalyzerHistory[1024];
    AudioAnalyzer txAnalyzer(txAnalyzerHistory, 1024, sampleRate);
    audioOutContext.setAnalyzer(&txAnalyzer);

    int16_t rxAnalyzerHistory[1024];
    AudioAnalyzer rxAnalyzer(rxAnalyzerHistory, 1024, sampleRate);
    audioInContext.setAnalyzer(&rxAnalyzer);

    LinkRootMachine rm(&ctx, &info, &audioOutContext);

    RXMonitor rxMonitor;

    // Cross-connects
    info.setAudioOut(&audioOutContext);
    audioInContext.setSink(&rxMonitor);
    ctx.setEventProcessor(&rm);
    rxMonitor.setSink(&rm);
    rxMonitor.setInfo(&info);

    //rm.setServerName(HostName("naeast.echolink.org"));
    //rm.setServerPort(5200);
    //rm.setCallSign(CallSign("W1TKZ-L"));
    //rm.setPassword(FixedString("xxx"));
    //rm.setFullName(FixedString("Wellesley Amateur Radio Society"));
    //rm.setLocation(FixedString("Wellesley, MA USA"));

    rm.setServerName(addressingServerHost);
    rm.setServerPort(addressingServerPort);
    rm.setCallSign(ourCallSign);
    rm.setPassword(ourPassword);
    rm.setFullName(ourFullName);
    rm.setLocation(ourLocation);

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

    bool rigCosState = false;
    uint32_t lastRigCosDetection = 0;
    bool rigKeyState = false;
    uint32_t lastRigKeyTransitionTime = 0;
    uint32_t rigKeyLockoutTime = 0;
    uint32_t rigKeyLockoutCount = 0;

    PicoPollTimer analyzerTimer;
    // Was 500000
    analyzerTimer.setIntervalUs(250000);
    bool analyzerOn = true;

    bool firstLoop = true;
    uint32_t lastConnectRequestMs = 0;
    uint32_t lastStopRequestMs = 0;

    // Last thing before going into the event loop
	watchdog_enable(WATCHDOG_DELAY_MS, true);

    cout << "Entering event loop" << endl;

    while (true) {

        // Keep things alive
        watchdog_update();

        cycleTimer.reset();

        // ----- External Controls ------------------------------------------

        bool rigCos = gpio_get(RIG_COS_PIN);
        // Look for activity on the line (not debounced)
        if (rigCos) {
            lastRigCosDetection = time_ms();
            rm.radioCarrierDetect();
        }

        // If the carrier is currently not detected
        if (rigCosState == false) {
            // The LO->HI transition is taken immediately
            if (rigCos) {
                if (info.getSquelch() || 
                    info.getMsSinceLastSquelchClose() < 500) {
                } 
                else {
                    info.setStatus("Rig COS on");
                    rigCosState = true;
                    if (rm.isInQSO()) {
                        rxMonitor.setForward(rigCosState);
                    }
                }
            }
        } 
        // If the carrier is currently active
        else {
            // The HI->LO transition is fully debounced
            if (!rigCos && 
                (time_ms() - lastRigCosDetection) > RIG_COS_DEBOUNCE_INTERVAL_MS) {
                if (!rigCos) {
                    info.setStatus("Rig COS off");
                }
                rigCosState = false;
                rxMonitor.setForward(rigCosState);
            }
        }

        // ----- Indicator Lights --------------------------------------------

        if (rxMonitor.getForward() || 
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
            else if (c == 'e') {
                cout << endl << "ESP32 Test: " <<  ctx.test() << endl;
            }
            else if (c == 'a') {
                audioOutContext.tone(500, 250);
            }
            else if (c == 'z') {
                audioOutContext.tone(800, 1000);
            }
            else if (c == 'i') {
                cout << endl;
                cout << "Diagnostics" << endl;
                cout << "Audio In Overflow : " << audioInContext.getOverflowCount() << endl;
                cout << "Audio Gain        : " << audioInContext.getGain() << endl;
                cout << "Max Skew (us)     : " << audioInContext.getMaxSkew() << endl;
                cout << "Max Len (us)      : " << audioInContext.getMaxLen() << endl;
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
                audioInContext.resetMax();
                audioInContext.resetOverflowCount();
                longestCycleUs = 0;
                longCycleCounter = 0;
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

        const float powerThreshold = 1e11;

        // Periodically do some analysis of the audio to find tones, etc.
        if (analyzerTimer.poll()) {

            bool dtmf_1209 = rxAnalyzer.getTonePower(1209) > powerThreshold; 
            bool dtmf_1336 = rxAnalyzer.getTonePower(1336) > powerThreshold; 
            bool dtmf_697 = rxAnalyzer.getTonePower(697) > powerThreshold; 

            bool oneActive = dtmf_1209 && dtmf_697;
            bool twoActive = dtmf_1336 && dtmf_697;

            // Analysis display
            //if (analyzerOn) {
            //    cout << rxAnalyzer.getRMS() << " " << rxAnalyzer.getPeakDBFS() << " dB " 
            //        << rxAnalyzer.getPeak() << endl;
            //}

            analyzerTimer.reset();

            if (oneActive) {
                if (time_ms() - lastConnectRequestMs > 2000) {
                    bool b = rm.connectToStation(CallSign("*ECHOTEST*"));
                    if (!b) {
                        lastConnectRequestMs = time_ms();
                    }
                }
            } else if (twoActive) {
                if (rm.isInQSO() && time_ms() - lastStopRequestMs > 2000) {
                    rm.requestCleanStop();  
                    lastStopRequestMs = time_ms();
                }
            }
        }

        // Deal with evreything that should be started once we are actually 
        // in the event loop.
        if (firstLoop) {
            audioInContext.setADCEnabled(true);
            audioInContext.resetMax();
            audioInContext.resetOverflowCount();
            // Start the state machine
            rm.start();
            firstLoop = false;
        }

        uint32_t ela = cycleTimer.elapsedUs();
        if (ela > longestCycleUs) {
            longestCycleUs = ela;
            cout << "Longest Cycle (us) " << longestCycleUs << endl;
        }
        if (ela > 40000) {
            longCycleCounter++;
        }
    }

    cout << "Left event loop" << endl;

    while (true) {        
    }
}
