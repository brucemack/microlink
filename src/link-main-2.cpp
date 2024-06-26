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
#include <functional>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"

// ======= Internet Stuff ===========
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "contexts/LwIPLib.h"
// ======= Internet Stuff ===========

/*
// ======= Internet Stuff ===========
#include "kc1fsz-tools/rp2040/PicoUartChannel.h"
#include "contexts/SIM7600IPLib.h"
// ======= Internet Stuff ===========
*/

#include "kc1fsz-tools/rp2040/SerialLog.h"
#include "kc1fsz-tools/AudioAnalyzer.h"
#include "kc1fsz-tools/DTMFDetector.h"
#include "kc1fsz-tools/OutStream.h"
#include "kc1fsz-tools/CommandShell.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"

#include "contexts/I2CAudioOutputContext.h"
#include "contexts/PicoAudioInputContext.h"

#include "machines/DNSMachine.h"
#include "machines/LogonMachine2.h"
#include "machines/LookupMachine3.h"

#include "RXMonitor.h"
#include "Conference.h"
#include "ConferenceBridge.h"
#include "SNTPClient.h"
// TEMP
#include "tests/TestUserInfo.h"

// Monitor Server
#define MONITOR_SERVER_NAME ("monitor.w1tkz.net")
// The time the raw COS needs to be active to be consered "on"
#define COS_DEBOUNCE_ON_MS 10
// The time the raw COS needs to be inactive to be considered "off"
#define COS_DEBOUNCE_OFF_MS 400
// The time the COS is ignore immediate after a TX cycle (to avoid 
// having the transmitter interfering with the receiver
#define LINGER_AFTER_TX_MS 500
// How long we wait in silence before flushing any accumualted DTMF
// tones.
#define DMTF_ACCUMULATOR_TIMEOUT_MS (10 * 1000)
// 
#define RIG_COS_DEBOUNCE_INTERVAL_MS (500)
// The longest the rig is allowed to stay on continuously
#define TX_TIMEOUT_MS (120 * 1000)
// A window of time during which the rig is not allowed to
// be keyed.
#define TX_LOCKOUT_MS (30 * 1000)
// A limit on the TX duty cycle - used to protect transmitter finals
#define TX_DUTY_CYCLE_LIMIT_PERCENT (50)
// How long we wait between refreshing the DNS address of the 
// servers used by the node.  Important to fail-over speed.
#define DNS_INTERVAL_MS (5 * 60 * 1000)

// ===============
// LEFT SIDE PINS 
// ===============

// Physical pin 1 - SIM7600 
#define UART0_TX_PIN (0)
// Physical pin 2 - SIM7600
#define UART0_RX_PIN (1)

// Physical pin 4 - SIM7600 enable
#define SIM7600_EN_PIN (2)

// Physical pin 9. Input from physical PTT button.
#define PTT_PIN (6)

// Physical pin 10. Output to drive an LED indicating keyed status
// Green LED on ML0 board
//#define KEY_LED_PIN (7)

// Physical pin 11 - Serial data logger
#define UART1_TX_PIN (8)
// Physical pin 12 - Serial data logger
#define UART1_RX_PIN (9)

// NOTE: Physical 13 is GND

// Physical pin 14 - Rig power on
#define RIG_POWER_PIN (10)

// Physical pin 15. This is an output to drive an LED indicating
// that we are in a QSO. 
#define QSO_LED_PIN (11)

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

// Output to drive an LED indicating link status
// Blue LED0 on ML1 board
#define LINK_LED_PIN (21)
// Output to drive an LED indicating keyed status
// Green LED1 on ML1 board
#define KEY_LED_PIN (22)

// Physical pin 31 - ADC input from analog section
#define ADC0_PIN (26)

// Physical pin 33 - Analog Ground

#define UART_ID uart0
#define U_BAUD_RATE 115200
#define U_DATA_BITS 8
#define U_STOP_BITS 1
#define U_PARITY UART_PARITY_NONE

// This controls the maximum delay before the watchdog
// will reboot the system
#define WATCHDOG_DELAY_MS (2 * 1000)

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

// Make the polling work like a Runnable
class NetworkTask : public Runnable {
public:
    void run() { 
        // If you are using pico_cyw43_arch_poll, then you must poll periodically 
        // from your main loop (not from a timer) to check for Wi-Fi driver or 
        // lwIP work that needs to be done.
        cyw43_arch_poll();
    }
};

class LinkOutStream : public OutStream {
public:

    virtual int write(uint8_t b) {
        cout.write((const char *)&b, 1);
        cout.flush();
        return 1;
    }

    virtual bool isWritable() const {
        return true;
    }
};

class LinkCommandSink : public CommandSink {
public:

    virtual void process(const char* cmd) {
        if (handler) {
            handler(cmd);
        }
    }

    std::function<void(const char*)> handler = 0;
};

// Used for drawing a real-time 
static void renderStatus(PicoAudioInputContext* inCtx,
    AudioAnalyzer* rxAnalyzer, 
    AudioAnalyzer* txAnalyzer, 
    uint32_t rxNoiseThreshold, 
    bool cosState, 
    bool networkState, 
    int wifiRssi, 
    uint32_t secondsSinceLastActivity,
    ostream& str);

bool rigKeyFailSafe();

static int32_t getInternetRssi() {
    int32_t rssi = 0;
    cyw43_wifi_get_rssi(&cyw43_state, &rssi);
    return rssi;
}

static bool streq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

static void saveConfig(const StationConfig* cfg) {
    uint32_t ints = save_and_disable_interrupts();
    // Must erase a full sector first (4096 bytes)
    flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    // IMPORTANT: Must be a multiple of 256!
    flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t*)cfg, 512);
    restore_interrupts(ints);
}

static void setDefaultConfig() {
    StationConfig config;
    config.version = CONFIG_VERSION;
    strncpy(config.addressingServerHost, "naeast.echolink.org", 32);
    config.addressingServerPort = 5200;
    strncpy(config.callSign, "UNKNOWN", 32);
    strncpy(config.password, "xxx", 32);
    strncpy(config.fullName, "Unknown", 32);
    strncpy(config.location, "Unknown", 32);
    strncpy(config.wifiSsid, "Unknown", 64);
    strncpy(config.wifiPassword, "Unknown", 16);
    config.useHardCos = false;
    config.silentTimeoutS = 30 * 60;
    config.idleTimeoutS = 5 * 60;
    config.rxNoiseThreshold = 2000;
    config.adcRawOffset = -166;
    config.cosDebounceOnMs = 10;
    config.cosDebounceOffMs = 400;
    saveConfig(&config);
}

int main(int, const char**) {

    LogonMachine2::traceLevel = 0;
    ConferenceBridge::traceLevel = 0;
    Conference::traceLevel = 0;

    stdio_init_all();

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

    // Link indicator LED
    gpio_init(LINK_LED_PIN);
    gpio_set_dir(LINK_LED_PIN, GPIO_OUT);
    gpio_put(LINK_LED_PIN, 0);

    // QSO indicator LED
    gpio_init(QSO_LED_PIN);
    gpio_set_dir(QSO_LED_PIN, GPIO_OUT);
    gpio_put(QSO_LED_PIN, 0);

    // Rig key
    gpio_init(RIG_KEY_PIN);
    gpio_set_dir(RIG_KEY_PIN, GPIO_OUT);
    gpio_put(RIG_KEY_PIN, 0);

    // Rig power
    gpio_init(RIG_POWER_PIN);
    gpio_set_dir(RIG_POWER_PIN, GPIO_OUT);
    gpio_put(RIG_POWER_PIN, 0);

    // Diag
    gpio_init(DIAG_PIN);
    gpio_set_dir(DIAG_PIN, GPIO_OUT);
    gpio_put(DIAG_PIN, 0);

    // UART1 setup (logging)
    uart_init(uart1, 9600);
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
    i2c_set_baudrate(i2c_default, 800000);

    // Hello indicator on boot
    for (int i = 0; i < 4; i++) {
        sleep_ms(250);
        sleep_ms(250);
    }

    SerialLog log(uart1);
    log.setStdout(true);

    log.info("===== MicroLink Link Station ============");
    log.info("Copyright (c) 2024 Bruce MacKinnon KC1FSZ");

    if (watchdog_caused_reboot()) {
        log.info("WATCHDOG REBOOT");
    } else if (watchdog_enable_caused_reboot()) {
        log.info("WATCHDOG EANBLE REBOOT");
    } else {
        log.info("Normal reboot");
    }

    // ----- READ CONFIGURATION FROM FLASH ------------------------------------

    // The very last sector of flash is used. Compute the memory-mapped address, 
    // remembering to include the offset for RAM
    const uint8_t* addr = (uint8_t*)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
    auto config = (const StationConfig*)addr;
    if (config->version != CONFIG_VERSION) {
        // Setup the default configuration 
        setDefaultConfig();        
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
    log.info("ADC offset           : %d", config->adcRawOffset);

    bool networkState = false;
    IPAddress currentIp;

    // ====== Internet Connectivity Stuff =====================================
    LwIPLib::traceLevel = 0;
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        log.error("Failed to initialize WIFI");
    } else {
        cyw43_arch_enable_sta_mode();
        //cyw43_arch_wifi_connect_async(config->wifiSsid, config->wifiPassword, 
        //    CYW43_AUTH_WPA2_AES_PSK);
    }
    LwIPLib ctx(&log);
    // ====== Internet Connectivity Stuff =====================================

    /*
    // ====== Internet Connectivity Stuff =====================================
    // UART0 setup (SIM7600)
    uart_init(uart0, 115200);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart0, false, false);
    uart_set_format(uart0, U_DATA_BITS, U_STOP_BITS, U_PARITY);
    uart_set_fifo_enabled(uart0, true);
    uart_set_translate_crlf(uart0, false);

    SIM7600IPLib::traceLevel = 1;
    uint8_t rxBufferArea[256];
    uint8_t txBufferArea[256];
    PicoUartChannel uartCtx(uart0, rxBufferArea, 256, txBufferArea, 256);
    SIM7600IPLib ctx(&log, &uartCtx, SIM7600_EN_PIN);
    ctx.reset();

    // SIM7600 module reset.
    // Per the schematic for the "hat" board, the reset is 
    // active high.
    //gpio_init(SIM7600_EN_PIN);
    //gpio_put(SIM7600_EN_PIN, 1);
    //gpio_set_dir(SIM7600_EN_PIN, GPIO_OUT);
    
    // ====== Internet Connectivity Stuff =====================================
    */

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
    radio0In.setRawOffset(config->adcRawOffset);

    // Analyzers for sound data
    int16_t txAnalyzerHistory[2048];
    AudioAnalyzer txAnalyzer(txAnalyzerHistory, 2048, sampleRate);
    radio0Out.setAnalyzer(&txAnalyzer);

    int16_t rxAnalyzerHistory[2048];
    AudioAnalyzer rxAnalyzer(rxAnalyzerHistory, 2048, sampleRate);
    radio0In.setAnalyzer(&rxAnalyzer);
    rxAnalyzer.setEnabled(true);

    int16_t dtmfDetectorHistory[400];
    DTMFDetector dtmfDetector(dtmfDetectorHistory, 400, sampleRate);

    // The RXMonitor is basically a gate between the rig's receiver
    // and the Conference.  
    RXMonitor rxMonitor;
    rxMonitor.setInfo(&info);
    // RXMonitor -> DTMF detector
    // Disabled for now for speed reasons.  Will review and optimize.
    //rxMonitor.setDTMFDetector(&dtmfDetector);

    // Radio RX -> RXMonitor
    radio0In.setSink(&rxMonitor);
    // ===== Audio Stuff ======================================================

    DNSMachine dnsMachine1(&ctx, &info, &log, DNS_INTERVAL_MS);
    ctx.addEventSink(&dnsMachine1);
    dnsMachine1.setHostName(ourAddressingServerHost);

    DNSMachine dnsMachine2(&ctx, &info, &log, DNS_INTERVAL_MS);
    ctx.addEventSink(&dnsMachine2);
    //dnsMachine2.setHostName(MONITOR_SERVER_NAME);

    // TODO: MOVE THIS TO CONFIG
    //FixedString versionId("1.06B");
    //FixedString emailAddr("bruce@mackinnon.com");
    FixedString versionId(VERSION_ID);
    FixedString emailAddr;

    LogonMachine2 logonMachine(&ctx, &log, &dnsMachine1, versionId);
    ctx.addEventSink(&logonMachine);
    logonMachine.setServerPort(config->addressingServerPort);
    logonMachine.setCallSign(ourCallSign);
    logonMachine.setPassword(ourPassword);
    logonMachine.setLocation(ourLocation);
    logonMachine.setEmailAddr(emailAddr);

    LookupMachine3 lookup(&ctx, &log);
    ctx.addEventSink(&lookup);
    lookup.setServerName(ourAddressingServerHost);
    lookup.setServerPort(config->addressingServerPort);

    ConferenceBridge confBridge(&ctx, &info, &log, &radio0Out);
    ctx.addEventSink(&confBridge);

    Conference conf(&lookup, &confBridge, &log, &dnsMachine1, &dnsMachine2);
    conf.setCallSign(ourCallSign);
    conf.setFullName(ourFullName);
    conf.setLocation(ourLocation);
    conf.setSilentTimeoutS(config->silentTimeoutS);
    conf.setIdleTimeoutS(config->idleTimeoutS);

    confBridge.setConference(&conf);
    lookup.setConference(&conf);
    rxMonitor.setSink(&confBridge);
    logonMachine.setConference(&conf);

    SNTPClient sntpClient(&log, &ctx);
    ctx.addEventSink(&sntpClient);

    bool rigKeyState = false;
    timestamp lastRigKeyTransitionTime;
    timestamp rigKeyLockoutTime;
    uint32_t rigKeyLockoutCount = 0;

    timestamp lastCosOn;
    timestamp lastCosTransition;
    bool cosState = false;
    bool lastCosState = false;

    int startupMode = 2;
    timestamp startupTime = time_ms();

    bool inContactWithMonitor = false;

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
    timestamp lastDtmfActivity;

    // Register the physical radio into the conference
    conf.addRadio(CallSign("RADIO0"), IPAddress(0xff000002));
    radio0In.setADCEnabled(true);
    
    NetworkTask wifiPollTask;

    // These are all of the tasks that need to be run on every iteration
    // of the main loop.
    const uint32_t taskCount = 13;
    Runnable* loopTasks[taskCount] = {
        &wifiPollTask,
        &ctx,
        &dnsMachine1,
        &dnsMachine2,
        &logonMachine,
        &lookup,
        &confBridge,
        // 7
        &conf,
        // 8
        &radio0Out,
        // 9
        &radio0In,
        // 10
        &rxMonitor,
        // 11
        &log,
        // 12
        &sntpClient
    };

    // Performance metrics
    uint32_t maxTaskTimeUs[taskCount];
    for (uint32_t i = 0; i < taskCount; i++)
        maxTaskTimeUs[i] = 0;
    PicoPerfTimer taskTimer;

    PicoPerfTimer cycleTimer;
    uint32_t longestCycleUs = 0;

    // ===== Command Shell Stuff =============================================

    enum ShellMode {
        LOG,
        PROMPT,
        STATUS,
        QUIT
    } shellMode = ShellMode::LOG;

    LinkOutStream shellOut;
    LinkCommandSink shellSink;
    CommandShell shell;
    shell.setOutput(&shellOut);
    shell.setSink(&shellSink);

    // Setup the command-handling function.  We do this as a lambda
    // so that we can access local variables in main().
    shellSink.handler = [&log, &shellMode, &radio0Out, &lookup, &conf, &txAnalyzer,
        &radio0In, &confBridge, &maxTaskTimeUs, &longestCycleUs, &rxAnalyzer,
        config]
        (const char* cmd)-> void {
        log.setStdout(true);
        if (streq(cmd, "quit")) {
            shellMode = ShellMode::QUIT;
        } 
        else if (streq(cmd, "log")) {
            shellMode = ShellMode::LOG;
        } 
        else if (streq(cmd, "reboot")) {
            // Hang the watchdog
            while (true);
        } 
        else if (streq(cmd, "tone")) {
            radio0Out.tone(800, 1000);
        }
        else if (streq(cmd, "dropall")) {
            conf.dropAll();
        }
        else if (streq(cmd, "status")) {
            shellMode = ShellMode::STATUS;
            txAnalyzer.setEnabled(true);
            cout << "\033[2J" << endl;
        }
        else if (strncmp(cmd, "add ", 4) == 0) { 
            // Parse
            FixedString tokens[2];
            uint32_t c = parseCommand(cmd, tokens, 2);
            if (c == 2) {
                tokens[1].toUpper();
                CallSign cs(tokens[1].c_str());
                IPAddress addr(0);
                StationID sid(addr, cs);
                lookup.validate(sid);
            }
        }
        else if (strncmp(cmd, "drop ", 5) == 0) { 
            // Parse
            FixedString tokens[2];
            uint32_t c = parseCommand(cmd, tokens, 2);
            if (c == 2) {
                tokens[1].toUpper();
                CallSign cs(tokens[1].c_str());
                conf.drop(cs);
            }
        }
        else if (strncmp(cmd, "set ", 4) == 0) { 
            // Parse
            FixedString tokens[3];
            uint32_t c = parseCommand(cmd, tokens, 3);
            if (c == 3) {
                StationConfig c;
                c.copyFrom(config);
                if (tokens[1] == "hardcos") {
                    c.useHardCos = atoi(tokens[2].c_str());
                }
                else if (tokens[1] == "wifissid") {
                    strcpyLimited(c.wifiSsid, tokens[2].c_str(), 64);
                }
                else if (tokens[1] == "wifipassword") {
                    strcpyLimited(c.wifiPassword, tokens[2].c_str(), 32);
                }
                else if (tokens[1] == "addressingserver") {
                    strcpyLimited(c.addressingServerHost, tokens[2].c_str(), 32);
                }
                else if (tokens[1] == "callsign") {
                    strcpyLimited(c.callSign, tokens[2].c_str(), 32);
                }
                else if (tokens[1] == "password") {
                    strcpyLimited(c.password, tokens[2].c_str(), 32);
                }
                else if (tokens[1] == "fullname") {
                    strcpyLimited(c.fullName, tokens[2].c_str(), 32);
                }
                else if (tokens[1] == "location") {
                    strcpyLimited(c.location, tokens[2].c_str(), 32);
                }
                else if (tokens[1] == "coson") {
                    c.cosDebounceOnMs = atol(tokens[2].c_str());
                }
                else if (tokens[1] == "cosoff") {
                    c.cosDebounceOffMs = atol(tokens[2].c_str());
                } 
                else if (tokens[1] == "costhreshold") {
                    c.rxNoiseThreshold = atol(tokens[2].c_str());
                } 
                else if (tokens[1] == "adcoffset") {
                    c.adcRawOffset = atol(tokens[2].c_str());
                    radio0In.setRawOffset(c.adcRawOffset);
                } 
                else {
                    cout << "Unrecognized configuration paramter" << endl;
                }
                saveConfig(&c);
            }
        }
        else if (streq(cmd, "info")) {

            cout << "Configuration:" << endl;
            config->dump(cout);
            cout << "Diagnostics:" << endl;
            cout << "Station count " << conf.getActiveStationCount() << endl;
            cout << "RX ADC value " << radio0In.getLastRawSample() << endl;
            cout << "Bridge overflow count " << confBridge.getRadio0GSMQueueOFCount() << endl;

            conf.dumpStations(&log);

            for (uint32_t t = 0; t < taskCount; t++) {
                cout << "Task " << t << " max time (us) " << maxTaskTimeUs[t] << endl;
                maxTaskTimeUs[t] = 0;
            }
            cout << "Max cycle (us) " << longestCycleUs << endl;
            longestCycleUs = 0;
        }
        else {
            cout << "Unreognized command " << cmd << endl;
        }

        log.setStdout(false);
    };

    // ------ Rig Key Duty Cycle Tracking Stuff ------------------------------

    // One slot every 5 seconds for 4 minutes
    const uint32_t rigKeyHistorySize = 48;
    bool rigKeyHistory[rigKeyHistorySize];
    uint32_t rigKeyHistoryPtr = 0;
    for (uint32_t i = 0; i < rigKeyHistorySize; i++)
        rigKeyHistory[i] = false;
    PicoPollTimer rigKeyHistoryTimer;
    rigKeyHistoryTimer.setIntervalUs(5'000'000);

    log.setStdout(true);

    // Last thing before going into the event loop
	watchdog_enable(WATCHDOG_DELAY_MS, true);

    log.info("Entering event loop");

    // TEMP TEMP TEMP
    inContactWithMonitor = true;

    while (shellMode != ShellMode::QUIT) {

        // Keep things alive
        watchdog_update();

        cycleTimer.reset();

        // Run the tasks, keeping track of the max time for each
        for (uint32_t t = 0; t < taskCount; t++) {
            taskTimer.reset();
            loopTasks[t]->run();
            maxTaskTimeUs[t] = std::max(maxTaskTimeUs[t], taskTimer.elapsedUs());
        }

        int c = getchar_timeout_us(0);

        // Serial shell processing 
        if (shellMode == ShellMode::PROMPT) {        
            if (c > 0) {
                shell.process((char)c);
                // If we entered log mode then make adjustments
                if (shellMode == ShellMode::LOG) {
                    log.setStdout(true);
                }
            }
        }
        else if (shellMode == ShellMode::LOG) {
            if (c > 0) {
                if (c == 27) {
                    shellMode = ShellMode::PROMPT;
                    log.setStdout(false);
                    shell.reset();
                }
            }
        }
        else if (shellMode == ShellMode::STATUS) {
            if (c == 27) {
                shellMode = ShellMode::LOG;
                log.setStdout(true);
                txAnalyzer.setEnabled(false);
                cout << "\033[2J" << endl;
            } 
            else {
                // Provide a live-updating dashboard of system status/audio/etc.
                if (renderTimer.poll()) {
                    renderTimer.reset();
                    renderStatus(&radio0In, &rxAnalyzer, &txAnalyzer, 
                        config->rxNoiseThreshold, cosState, 
                        networkState,
                        getInternetRssi(),
                        conf.getSecondsSinceLastActivity(),
                        cout);
                }
            }
        }

        // ----- Look for periodic reboot ----------------------------------------
        // We restart once every 24 hours, but only if the system has 
        // been quiet for a few minutes.
        if (conf.getSecondsSinceLastActivity() > 2 * 60 &&
            ms_since(startupTime) > (24 * 60 * 60 * 1000)) {
            log.info("Automatic reboot");
            // The watchdog will take over from here
            while (true);
        } 

        // ----- Deal with Inbound DTMF Requests ---------------------------------

        if (dtmfAccumulatorLen > 0 &&
            ms_since(lastDtmfActivity) > DMTF_ACCUMULATOR_TIMEOUT_MS) {
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
            if (ms_since(startupTime) > 1000) {
                int16_t rawSample = radio0In.getLastRawSample();
                log.info("Raw sample %d", rawSample);
                int16_t avg = rxAnalyzer.getAvg();
                log.info("Baseline DC bias (V) %d", avg);
                radio0In.resetMax();
                radio0In.resetOverflowCount();
                startupMode = 0;
            }
        } 

        // ----- Monitor State -----------------------------------------------

        // This is a dead-man's switch.  If the monitor isn't pinging us then 
        // we are out of contact and certain functions become disabled.
        if (!inContactWithMonitor) {
            if (conf.getSecondsSinceLastMonitorRx() < 60) {
                log.info("In contact with monitor");
                inContactWithMonitor = true;
            }
        } else {
            //if (conf.getSecondsSinceLastMonitorRx() >= 60) {
            //    log.info("Lost contact with monitor, no rig allowed");
            //    inContactWithMonitor = false;
            //}
        }

        // ----- Rig Power Management -------------------------------------------

        if (inContactWithMonitor)
            gpio_put(RIG_POWER_PIN, 1);
        else
            gpio_put(RIG_POWER_PIN, 0);

        // ----- Rig Key Management -------------------------------------------
        //
        // Key rig when audio is coming in, but enforce limits to prevent
        // the key from being stuck open for long periods.

        // Calculate the percentage keyed so that we can manage 
        // overheating
        uint32_t rigKeyCount = 0;
        for (uint32_t i = 0; i < rigKeyHistorySize; i++)
            if (rigKeyHistory[i])
                rigKeyCount++;
        const uint32_t rigKeyPercent = (100 * rigKeyCount) / rigKeyHistorySize;
        // Check duty cycle threshold (50%)
        const bool rigKeyOverheat = rigKeyPercent > TX_DUTY_CYCLE_LIMIT_PERCENT;

        if (!rigKeyState) {
            // This logic will key the rig when (a) we are sending audio
            // to the rig and (b) we are not inside of the "lockout 
            // interval."
            if (radio0Out.getSquelch() && 
                ms_since(rigKeyLockoutTime) > TX_LOCKOUT_MS &&
                !rigKeyOverheat) {
                if (!inContactWithMonitor) {
                    // Not allowed to key
                }
                else {
                    log.info("Keying radio (duty cycle %lu%%)", 
                        rigKeyPercent);
                    rigKeyState = true;
                    lastRigKeyTransitionTime = time_ms();
                }
            }
        }
        else {

            // Check for normal unkey when we've stopped streaming audio to 
            // the rig.
            if (!info.getSquelch()) {
                rigKeyState = false;
                lastRigKeyTransitionTime = time_ms();
                log.info("Unkeying radio (duty cycle %lu%%)", rigKeyPercent);
            }
            // Look for timeout case where the rig has been keyed for too long
            else if (ms_since(lastRigKeyTransitionTime) > TX_TIMEOUT_MS) {
                log.info("TX timeout triggered");
                rigKeyState = false;
                lastRigKeyTransitionTime = time_ms();
                rigKeyLockoutTime = time_ms();
                rigKeyLockoutCount++;
            }
            // Look for overheating
            else if (rigKeyOverheat) {
                log.info("Radio duty cycle exceeded (%lu%%)", 
                    rigKeyPercent);
                rigKeyState = false;
                lastRigKeyTransitionTime = time_ms();
            }
        }

        // *****************************************************************
        // *****************************************************************
        // Here is where we actually control the rig key.
        bool finalRigKeyState = rigKeyFailSafe() && rigKeyState;
        gpio_put(RIG_KEY_PIN, finalRigKeyState ? 1 : 0);
        // *****************************************************************
        // *****************************************************************

        // Collect rig key state over time so that we can calculate duty
        // cycle.
        if (rigKeyHistoryTimer.poll()) {
            rigKeyHistoryTimer.reset();
            rigKeyHistory[rigKeyHistoryPtr++] = finalRigKeyState;
            // Wrap if necessary
            if (rigKeyHistoryPtr == rigKeyHistorySize) {
                rigKeyHistoryPtr = 0;
            }
            // Display that status if we are overheated
            if (rigKeyOverheat) {
                log.info("Limiting duty cycle, current percent %lu%%",
                    rigKeyPercent);
            }
        }

        // ----- Rig Carrier Detect Management --------------------------------
        //
        // There are two ways supported:
        // 1. Hard COS: explicit signal from rig (preferred)
        // 2. Soft COS: thresholding noise level on receiver

        bool rigCosState = (config->useHardCos) ? 
            gpio_get(RIG_COS_PIN) : 
            startupMode == 0 && 
                rxAnalyzer.getMS() > config->rxNoiseThreshold;

        // Produce a debounced cosState, which indicates the state of
        // the carrier detect.
        //
        // Look for LOW->HI transition
        if (rigCosState && cosState == false) {
            // Debounce.  The LOW->HI transition is taken very quickly,
            // so long as we are not just finishing up a transmission.
            if (ms_since(lastCosTransition) > LINGER_AFTER_TX_MS &&
                !info.getSquelch() &&
                info.getMsSinceLastSquelchClose() > LINGER_AFTER_TX_MS) {
                cosState = true;
            }
        }
        // Look for HI->LOW transition
        else if (!rigCosState && cosState == true) {
            // The HI->LO transition is fully debounced and is less
            // agressive.
            if (ms_since(lastCosOn) > COS_DEBOUNCE_OFF_MS) {
                cosState = false;
            }
        }

        if (rigCosState)
            lastCosOn = time_ms();

        // Use the debounced cosState to adjust the state of the node
        if (cosState != lastCosState) {
            if (cosState) 
                log.info("Radio COS on");
            else
                log.info("Radio COS off");
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

        // Maintenance that happens on a slow pol

        if (secondTimer.poll()) {
            secondTimer.reset();

            int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

            if (st == CYW43_LINK_JOIN) {
                if (!networkState) {
                    log.info("The Internet is up");
                    gpio_put(LINK_LED_PIN, 1);
                }
                networkState = true;
            }
            else {
                if (networkState) {
                    log.info("The Internet is down");
                    gpio_put(LINK_LED_PIN, 0);
                }
                networkState = false;
            }

            if (st == CYW43_LINK_DOWN || st == CYW43_LINK_FAIL) {
                log.info("Attempting to connect to WIFI");
                cyw43_arch_wifi_connect_async(config->wifiSsid, config->wifiPassword, 
                    CYW43_AUTH_WPA2_AES_PSK);
            }

            // Get the IP address and see if it's changed 
            IPAddress myIp(ntohl(cyw43_state.netif[0].ip_addr.addr));
            if (!(currentIp == myIp)) {
                char buf[32];
                myIp.formatAsDottedDecimal(buf, 32);
                log.info("Assigned IP address: %s", buf);
                currentIp = myIp;
            }

            // Pass some information into the Conference for the 
            // dianostic messages
            conf.setWifiRssi(getInternetRssi());
            conf.setRxPower(rxAnalyzer.getMS());
            conf.setRxSample(radio0In.getLastRawSample());
        }

        // The key LED is steady when COS is enabled and flashing when
        // the rig is keyed
        if (rigKeyState) {
            gpio_put(KEY_LED_PIN, flashState);
        } else if (cosState) {
            gpio_put(KEY_LED_PIN, 1);
        } else {
            gpio_put(KEY_LED_PIN, 0);
        }

        uint32_t ela = cycleTimer.elapsedUs();
        if (ela > longestCycleUs) {
            longestCycleUs = ela;
            log.info("Longest Cycle (us) %lu", longestCycleUs);
        }
    }    

    log.setStdout(true);
    log.info("Out of loop");

    gpio_put(RIG_KEY_PIN, 0);

    while (true) {
        // Keep things alive
        watchdog_update();
    }

    return 0;
}

static void renderStatus(PicoAudioInputContext* inCtx,
    AudioAnalyzer* rxAnalyzer, 
    AudioAnalyzer* txAnalyzer, 
    uint32_t rxNoiseThreshold, 
    bool cosState, 
    bool networkState, int wifiRssi, 
    uint32_t secondsSinceLastActivity,
    ostream& str) {

    // [K - Erase line
    // [2J - Clear screen
    // [H - Home
    char ESC = '\033';

    std::ios_base::fmtflags f(str.flags());

    str << ESC << "[H";
    str << "===== MicoLink Status =====";
    str << endl << ESC << "[K";
    str << endl << ESC << "[K";
    str << "    Activity (sec) : " << secondsSinceLastActivity;
    str << endl << ESC << "[K";
    str << "              WIFI : " << (networkState ? "Yes" : "No");
    str << endl << ESC << "[K";
    str << "         WIFI RSSI : " << wifiRssi;
    str << endl << ESC << "[K";
    str << "               COS : " << (cosState ? "Yes" : "No");
    str << endl << ESC << "[K";
    str << "     RX Raw Sample : " << inCtx->getLastRawSample();
    str << endl << ESC << "[K";
    str << "    RX Audio Power : " << rxAnalyzer->getMS();
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

/**
 * This function should contain special logic to make sure that the 
 * rig isn't keyed unexpectedly. The most important part of an 
 * application like this is making sure that we don't accidenally
 * leave the radio key-down.
 */
bool rigKeyFailSafe() {
    return true;
}
