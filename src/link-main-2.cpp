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
Build commands:

    cd build
    cmake -DPICO_BOARD=pico_w ..
    make link-main-2

Launch command:
    openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program link-main-2.elf verify reset exit"
*/

#include <ios>
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
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"
#include "pico/cyw43_arch.h"

#include "kc1fsz-tools/rp2040/SerialLog.h"
#include "kc1fsz-tools/AudioAnalyzer.h"
#include "kc1fsz-tools/DTMFDetector.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"

#include "contexts/I2CAudioOutputContext.h"
#include "contexts/PicoAudioInputContext.h"
#include "contexts/LwIPLib.h"

#include "machines/LogonMachine2.h"
#include "machines/LookupMachine3.h"

#include "RXMonitor.h"
#include "Conference.h"
#include "ConferenceBridge.h"
// TEMP
#include "tests/TestUserInfo.h"

// ===============
// LEFT SIDE PINS 
// ===============

// Physical pin 9. Input from physical PTT button.
#define PTT_PIN (6)
// Physical pin 10. Output to drive an LED indicating keyed status
#define KEY_LED_PIN (7)

// Physical pin 11 - Serial data logger
#define UART1_TX_PIN (8)
// Physical pin 12 - Serial data logger
#define UART1_RX_PIN (9)

// NOTE: Physical 13 is GND

// Physical pin 15. This is an output to drive an LED indicating
// that we are in a QSO. 
#define QSO_LED_PIN (11)

// Physical pin 16. Ouptut to hard reset on ESP32 (EN on DEVKITV1)
//#define ESP_EN_PIN (12)

// Physical pin 20.  Used for diagnostics/timing checks
#define DIAG_PIN (15)

// ===============
// RIGHT SIDE PINS 
// ===============

// Phy Pin 21: I2C channel 0 - DAC data
#define I2C0_SDA (16) 
// Phy Pin 22: I2C channel 0 - DAC clock
#define I2C0_SCL (17) 

// Physical pin 23 - GROUND

// Physical pin 24.  This is an output (active high) used to key 
// the rig's transmitter. Typically drives an optocoupler to
// get the pull-to-ground needed by the rig.
#define RIG_KEY_PIN (18)
// Physical pin 25. This is an input (active high) used to detect
// receive carrier from the rig. 
#define RIG_COS_PIN (19)

// No physical pin on board
//#define LED_PIN (25)

// Physical pin 31 - ADC input from analog section
#define ADC0_PIN (26)

// Physical pin 33 - Analog Ground

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

// The time the raw COS needs to be active to be consered "on"
#define COS_DEBOUNCE_ON_MS 5
// The time the raw COS needs to be inactive to be considered "off"
#define COS_DEBOUNCE_OFF_MS 250
// The time the COS is ignore immediate after a TX cycle (to avoid 
// having the transmitter interfering with the receiver
#define LINGER_AFTER_TX_MS 500
// How long we wait in silence before flushing any accumualted DTMF
// tones.
#define DMTF_ACCUMULATOR_TIMEOUT_MS (10 * 1000)
// The version of the configuration version that we expect 
// to find (must be at least this version)
#define CONFIG_VERSION (0xbab2)

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

// Used for drawing a real-time 
static void renderStatus(PicoAudioInputContext* inCtx,
    AudioAnalyzer* rxAnalyzer, 
    AudioAnalyzer* txAnalyzer, int16_t baselineRxNoise, 
    uint32_t rxNoiseThreshold, bool cosState, 
    bool wifiState, int wifiRssi, 
    ostream& str);

int main(int, const char**) {

    LogonMachine2::traceLevel = 0;
    LwIPLib::traceLevel = 0;
    ConferenceBridge::traceLevel = 0;
    Conference::traceLevel = 0;

    // Seup PICO
    stdio_init_all();

    // On-board LED
    //gpio_init(LED_PIN);
    //gpio_set_dir(LED_PIN, GPIO_OUT);

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

    // Rig key
    gpio_init(RIG_KEY_PIN);
    gpio_set_dir(RIG_KEY_PIN, GPIO_OUT);
    gpio_put(RIG_KEY_PIN, 0);

    // Diag
    gpio_init(DIAG_PIN);
    gpio_set_dir(DIAG_PIN, GPIO_OUT);
    gpio_put(DIAG_PIN, 0);
       
    // UART1 setup (logging)
    uart_init(uart1, U_BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart1, false, false);
    uart_set_format(uart1, U_DATA_BITS, U_STOP_BITS, U_PARITY);
    uart_set_fifo_enabled(uart1, true);
    uart_set_translate_crlf(uart1, false);

    // Setup I2C
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    //i2c_set_baudrate(i2c_default, 400000 * 4);
    i2c_set_baudrate(i2c_default, 800000);

    // Hello indicator
    for (int i = 0; i < 4; i++) {
    //    gpio_put(LED_PIN, 1);
        sleep_ms(250);
    //    gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }

    SerialLog log(uart1);
    log.setStdout(true);

    log.info("===== MicroLink Link Station ============");
    log.info("Copyright (C) 2024 Bruce MacKinnon KC1FSZ");

    if (watchdog_caused_reboot()) {
        log.info("WATCHDOG REBOOT");
    } else if (watchdog_enable_caused_reboot()) {
        log.info("WATCHDOG EANBLE REBOOT");
    } else {
        log.info("Normal reboot");
    }
    /*
    // TEMPORARY!
    {
        // Write flash
        StationConfig config;
        config.version = CONFIG_VERSION;
        strncpy(config.addressingServerHost, "naeast.echolink.org", 32);
        config.addressingServerPort = 5200;
        strncpy(config.callSign, "W1TKZ-L", 32);
        strncpy(config.password, "xxx", 32);
        strncpy(config.fullName, "Wellesley Amateur Radio Society", 32);
        strncpy(config.location, "Wellesley, MA USA", 32);
        strncpy(config.wifiSsid, "Gloucester Island Municipal WIFI", 64);
        strncpy(config.wifiPassword, "xxx", 16);
        config.useHardCos = false;
        config.silentTimeoutS = 30 * 60;
        config.idleTimeoutS = 5 * 60;
        config.rxNoiseThreshold = 50;

        uint32_t ints = save_and_disable_interrupts();
        // Must erase a full sector first (4096 bytes)
        flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
        // IMPORTANT: Must be a multiple of 256!
        flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t*)&config, 512);
        restore_interrupts(ints);
    } 
    */

    // ----- READ CONFIGURATION FROM FLASH ------------------------------------

    // The very last sector of flash is used. Compute the memory-mapped address, 
    // remembering to include the offset for RAM
    const uint8_t* addr = (uint8_t*)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
    auto config = (const StationConfig*)addr;
    if (config->version != CONFIG_VERSION) {
        log.error("Configuration data is invalid");
        panic_unsupported();
        return -1;
    } 

    HostName ourAddressingServerHost(config->addressingServerHost);
    CallSign ourCallSign;
    FixedString ourPassword;
    FixedString ourFullName;
    FixedString ourLocation;
    ourCallSign = CallSign(config->callSign);
    ourPassword = FixedString(config->password);
    ourFullName = FixedString(config->fullName);
    ourLocation = FixedString(config->location);

    log.info("EL Addressing Server : %s:%lu", ourAddressingServerHost.c_str(),
        config->addressingServerPort);
    log.info("Identification       : %s/%s/%s", ourCallSign.c_str(),
        ourFullName.c_str(), ourLocation.c_str());
    log.info("Idle Timeout (s)     : %d", config->idleTimeoutS);
    log.info("Silent Timeout (s)   : %d", config->silentTimeoutS);

    // ====== Internet Connectivity Stuff =====================================
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        log.error("Failed to initialize WIFI");
    } else {
        cyw43_arch_enable_sta_mode();
        cyw43_arch_wifi_connect_async(config->wifiSsid, config->wifiPassword, 
            CYW43_AUTH_WPA2_AES_PSK);
    }
    LwIPLib ctx(&log);
    bool wifiState = false;
    // ====== Internet Connectivity Stuff =====================================

    TestUserInfo info;

    // ===== Audio Stuff ======================================================
    // NOTE: Audio is encoded and decoded in 4-frame chunks.
    I2CAudioOutputContext radio0Out(audioFrameSize * 4, sampleRate, 
        audioBufDepthLog2, audioBuf, &info);
    // ADC/audio in setup
    PicoAudioInputContext::setup();
    PicoAudioInputContext radio0In;
    // Connect the input (ADC) timer to the output (DAC)
    radio0In.setSampleCb(I2CAudioOutputContext::tickISR, &radio0Out);

    // Analyzers for sound data
    int16_t txAnalyzerHistory[2048];
    AudioAnalyzer txAnalyzer(txAnalyzerHistory, 2048, sampleRate);
    radio0Out.setAnalyzer(&txAnalyzer);

    int16_t rxAnalyzerHistory[2048];
    AudioAnalyzer rxAnalyzer(rxAnalyzerHistory, 2048, sampleRate);
    radio0In.setAnalyzer(&rxAnalyzer);

    int16_t dtmfDetectorHistory[400];
    DTMFDetector dtmfDetector(dtmfDetectorHistory, 400, sampleRate);

    // The RXMonitor is basically a gate between the rig's receiver
    // and the Conference.  
    RXMonitor rxMonitor;
    rxMonitor.setInfo(&info);
    // RXMonitor -> DTMF detector
    rxMonitor.setDTMFDetector(&dtmfDetector);

    // Radio RX -> RXMonitor
    radio0In.setSink(&rxMonitor);
    // ===== Audio Stuff ======================================================

    LogonMachine2 logonMachine(&ctx, &info, &log);
    logonMachine.setServerName(ourAddressingServerHost);
    logonMachine.setServerPort(config->addressingServerPort);
    logonMachine.setCallSign(ourCallSign);
    logonMachine.setPassword(ourPassword);
    logonMachine.setLocation(ourLocation);

    LookupMachine3 lookup(&ctx, &info, &log);
    lookup.setServerName(ourAddressingServerHost);
    lookup.setServerPort(config->addressingServerPort);

    ConferenceBridge confBridge(&ctx, &info, &log, &radio0Out);

    Conference conf(&lookup, &confBridge, &log);
    conf.setCallSign(ourCallSign);
    conf.setFullName(ourFullName);
    conf.setLocation(ourLocation);
    conf.setSilentTimeoutS(config->silentTimeoutS);
    conf.setIdleTimeoutS(config->idleTimeoutS);

    confBridge.setConference(&conf);
    lookup.setConference(&conf);
    ctx.addEventSink(&logonMachine);
    ctx.addEventSink(&lookup);
    ctx.addEventSink(&confBridge);
    rxMonitor.setSink(&confBridge);
    logonMachine.setConference(&conf);

    bool rigKeyState = false;
    uint32_t lastRigKeyTransitionTime = 0;
    uint32_t rigKeyLockoutTime = 0;
    uint32_t rigKeyLockoutCount = 0;

    uint32_t lastCosOn = 0;
    bool cosState = false;
    bool lastCosState = false;
    uint32_t lastCosTransition = 0;

    int startupMode = 2;
    uint32_t startupMs = time_ms();
    int16_t baselineRxNoise = 0;

    bool statusPage = false;
    PicoPollTimer renderTimer;
    renderTimer.setIntervalUs(500000);
    PicoPollTimer flashTimer;
    flashTimer.setIntervalUs(250 * 1000);
    bool flashState = false;
    PicoPollTimer secondTimer;
    secondTimer.setIntervalUs(1000 * 1000);

    const uint32_t dtmfAccumulatorSize = 16;
    char dtmfAccumulator[dtmfAccumulatorSize];
    uint32_t dtmfAccumulatorLen = 0;
    uint32_t lastDtmfActivity = 0;

    // Register the physical radio into the conference
    conf.addRadio(CallSign("RADIO0"), IPAddress(0xff000002));
    radio0In.setADCEnabled(true);

    // Last thing before going into the event loop
	watchdog_enable(WATCHDOG_DELAY_MS, true);

    log.info("Entering event loop");

    while (true) {

        // Keep things alive
        watchdog_update();

        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();

        ctx.run();
        logonMachine.run();
        lookup.run();
        confBridge.run();
        conf.run();
        radio0Out.run();
        radio0In.run();
        rxMonitor.run();

        // ----- Serial Commands ---------------------------------------------
        
        int c = getchar_timeout_us(0);
        if (c > 0) {
            if (c == 'q') {
                break;
            } 
            else if (c == 'z') {
                radio0Out.tone(800, 1000);
            }
            else if (c == 'd') {
                conf.dropAll();
            } else if (c == 'a') {
                CallSign cs("*ECHOTEST*");
                IPAddress addr(0);
                StationID sid(addr, cs);
                lookup.validate(sid);
            }
            else if (c == 'o') {
                if (statusPage) {
                log.setStdout(true);
                    statusPage = false;
                    cout << "\033[2J" << endl;
                } else {
                    log.setStdout(false);
                    statusPage = true;
                    cout << "\033[2J";
                }
            }
            else if (c == 'i') {
                log.info("Station Count: %d", conf.getActiveStationCount());
                conf.dumpStations(&log);
            }
        }

        // ----- Deal with Inbound DTMF Requests ---------------------------------

        if (dtmfAccumulatorLen > 0 &&
            time_ms() - lastDtmfActivity > DMTF_ACCUMULATOR_TIMEOUT_MS) {
            dtmfAccumulatorLen = 0;
            log.info("Discarding DTMF activity");
        }

        while (dtmfDetector.resultAvailable()) {
            char c = dtmfDetector.getResult();
            log.info("DTMF: %c", c);
            if (dtmfAccumulatorLen < dtmfAccumulatorSize)
                dtmfAccumulator[dtmfAccumulatorLen++] = c;
            lastDtmfActivity = time_ms();
        }

        if (dtmfAccumulatorLen >= 2) {
            if (dtmfAccumulator[0] == '1' and dtmfAccumulator[1] == '4') {
            }
            else if (dtmfAccumulator[0] == '1' and dtmfAccumulator[1] == '7') {
            }
        }

        // ----- Calibration --------------------------------------------------

        // At startup we wait some time to adjust a few parameters before 
        // opening the state machines for connections.
        if (startupMode == 2) {
            if (time_ms() > startupMs + 500) {
                int16_t avg = rxAnalyzer.getAvg();
                log.info("Basline DC bias (V) %d", -avg);
                radio0In.addBias(-avg);
                radio0In.resetMax();
                radio0In.resetOverflowCount();
                startupMode = 1;
                startupMs = time_ms();
            }
        } 
        else if (startupMode == 1) {
            if (time_ms() > startupMs + 500) {
                baselineRxNoise = rxAnalyzer.getRMS();
                log.info("Baseline RX noise (Vrms) %d", baselineRxNoise);
                startupMode = 0;
            }
        }

        // ----- Rig Key Management -------------------------------------------
        //
        // Key rig when audio is coming in, but enforce limits to prevent
        // the key from being stuck open for long periods.

        if (!rigKeyState) {
            if (radio0Out.getSquelch() && 
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

        // ----- Rig Carrier Detect Management --------------------------------
        //
        // There are two ways supported:
        // 1. Hard COS: explicit signal from rig (preferred)
        // 2. Soft COS: thresholding noise level on receiver

        bool rigCosState = (config->useHardCos) ? 
            gpio_get(RIG_COS_PIN) : 
            (rxAnalyzer.getRMS() - baselineRxNoise) > (int16_t)config->rxNoiseThreshold;

        // Produce a debounced cosState, which indicates the state of
        // the carrier detect.
        //
        // Look for LOW->HI transition
        if (rigCosState && cosState == false) {
            // Debounce.  The LOW->HI transition is taken very quickly,
            // so long as we are not just finishing up a transmission.
            if ((time_ms() - lastCosTransition) > LINGER_AFTER_TX_MS &&
                !info.getSquelch() &&
                info.getMsSinceLastSquelchClose() > LINGER_AFTER_TX_MS) {
                cosState = true;
            }
        }
        // Look for HI->LOW transition
        else if (!rigCosState && cosState == true) {
            // The HI->LO transition is fully debounced and is less
            // agressive.
            if ((time_ms() - lastCosOn) > COS_DEBOUNCE_OFF_MS) {
                cosState = false;
            }
        }

        if (rigCosState)
            lastCosOn = time_ms();

        // Use the debounced cosState to adjust the state of the node
        if (cosState != lastCosState) {
            if (cosState) 
                info.setStatus("Rig COS on");
            else
                info.setStatus("Rig COS off");
            // This is the important part: it turns on the forwarding from 
            // the readio into the Conference.            
            rxMonitor.setForward(cosState);
            lastCosState = cosState;
            lastCosTransition = time_ms();
        }

        // ----- UI Rendering ------------------------------------------------

        if (flashTimer.poll()) {
            flashTimer.reset();
            flashState = !flashState;
        }

        // Displays that happen on a slow pol
        if (secondTimer.poll()) {
            secondTimer.reset();

            if (ctx.isLinkUp()) {
                if (!wifiState) {
                    log.info("WIFI is up");
                }
                wifiState = true;
            } else {
                if (wifiState) {
                    log.info("WIFI is down");
                }
                wifiState = false;
            }
        }

        // The key LED is steady when the rig key is down and flashing 
        // when the rig COS is enabled
        if (rigKeyState) {
            gpio_put(KEY_LED_PIN, 1);
        } else if (cosState) {
            gpio_put(KEY_LED_PIN, flashState);
        } else {
            gpio_put(KEY_LED_PIN, 0);
        }

        // Provide a live-updating dashboard of system status/audio/etc.
        if (statusPage) {
            if (renderTimer.poll()) {
                renderTimer.reset();
                int32_t rssi = 0;
                cyw43_wifi_get_rssi(&cyw43_state, &rssi);
                renderStatus(&radio0In, &rxAnalyzer, &txAnalyzer, 
                    baselineRxNoise, config->rxNoiseThreshold, cosState, 
                    wifiState,
                    rssi,
                    cout);
            }
        }
    }    

    log.info("Out of loop");

    while (true) {
        // Keep things alive
        watchdog_update();
    }

    return 0;
}

static void renderStatus(PicoAudioInputContext* inCtx,
    AudioAnalyzer* rxAnalyzer, 
    AudioAnalyzer* txAnalyzer, int16_t baselineRxNoise, 
    uint32_t rxNoiseThreshold, bool cosState, 
    bool wifiState, int wifiRssi, ostream& str) {

    // [K - Erase line
    // [2J - Clear screen
    // [H - Home
    char ESC = '\033';

    std::ios_base::fmtflags f(str.flags());

    str << ESC << "[H";
    str << "===== MicoLink Status =====";
    str << endl << ESC << "[K";
    str << endl << ESC << "[K";
    str << "              WIFI : " << (wifiState ? "Yes" : "No");
    str << endl << ESC << "[K";
    str << "         WIFI RSSI : " << wifiRssi;
    str << endl << ESC << "[K";
    str << "               COS : " << (cosState ? "Yes" : "No");
    str << endl << ESC << "[K";
    str << "          RX Level : " << (int)rxAnalyzer->getRMS();
    str << endl << ESC << "[K";
    str << "   RX Level Excess : " << rxAnalyzer->getRMS() - baselineRxNoise;
    str << endl << ESC << "[K";
    str << "RX Noise Threshold : " << rxNoiseThreshold;
    str << endl << ESC << "[K";
    str << "    RX Audio Peak% : " << rxAnalyzer->getPeakPercent();
    str << endl << ESC << "[K";
    str << "      RX Audio Avg : " << rxAnalyzer->getAvg();
    str << endl << ESC << "[K";
    str.setf(ios::fixed,ios::floatfield);
    str.precision(1);    
    str << "RX Audio Peak dBFS : " << rxAnalyzer->getPeakDBFS();
    str.flags(f);
    str << endl << ESC << "[K";
    str << "    TX Audio Power : " << (int)txAnalyzer->getRMS();
    str << endl << ESC << "[K";
    str << "   TX Audio Peak%  : " << txAnalyzer->getPeakPercent();
    str << endl << ESC << "[K";
    str << "      TX Audio Avg : " << txAnalyzer->getAvg();
    str << endl << ESC << "[K";
    str.setf(ios::fixed,ios::floatfield);
    str.precision(1);    
    str << "TX Audio Peak dBFS : " << txAnalyzer->getPeakDBFS();
    str.flags(f);
    str << endl << ESC << "[K";
    str << " Audio In Overflow : " << inCtx->getOverflowCount();
    str << endl << ESC << "[K";
    str << "     Max Skew (us) : " << inCtx->getMaxSkew();
    str << endl << ESC << "[K";
    str << "      Max Len (us) : " << inCtx->getMaxLen();
}
