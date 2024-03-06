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

#include "contexts/LwIPLib.h"
#include "machines/LogonMachine2.h"
#include "machines/LookupMachine3.h"
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
#define CONFIG_VERSION (0xbab1)

using namespace std;
using namespace kc1fsz;

int main(int, const char**) {

    LogonMachine2::traceLevel = 0;
    LwIPLib::traceLevel = 1;
    ConferenceBridge::traceLevel = 1;
    //Conference::traceLevel = 1;

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

    // ----- READ CONFIGURATION FROM FLASH ------------------------------------

    HostName addressingServerHost;
    uint32_t addressingServerPort;
    CallSign ourCallSign;
    FixedString ourPassword;
    FixedString ourFullName;
    FixedString ourLocation;
    uint32_t rxNoiseThreshold = 50;
    bool useHardCos = true;

    // The very last sector of flash is used. Compute the memory-mapped address, 
    // remembering to include the offset for RAM
    const uint8_t* addr = (uint8_t*)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
    auto p = (const StationConfig*)addr;
    if (p->version != CONFIG_VERSION) {
        log.error("Configuration data is invalid");
        panic_unsupported();
        return -1;
    } 

    addressingServerHost = HostName(p->addressingServerHost);
    addressingServerPort = p->addressingServerPort;
    ourCallSign = CallSign(p->callSign);
    ourPassword = FixedString(p->password);
    ourFullName = FixedString(p->fullName);
    ourLocation = FixedString(p->location);

    log.info("EL Addressing Server : %s:%lu", addressingServerHost.c_str(),
        addressingServerPort);
    log.info("Identification       : %s/%s/%s", ourCallSign.c_str(),
        ourFullName.c_str(), ourLocation.c_str());

    // ====== Internet Connectivity Stuff =====================================
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        log.error("Failed to initialize WIFI");
    } else {
        cyw43_arch_enable_sta_mode();
        cyw43_arch_wifi_connect_async(p->wifiSsid, p->wifiPassword, CYW43_AUTH_WPA2_AES_PSK);
    }
    LwIPLib ctx(&log);
    // ====== Internet Connectivity Stuff =====================================

    TestUserInfo info;

    LogonMachine2 lm(&ctx, &info, &log);
    lm.setServerName(addressingServerHost);
    lm.setServerPort(addressingServerPort);
    lm.setCallSign(ourCallSign);
    lm.setPassword(ourPassword);
    lm.setLocation(ourLocation);

    LookupMachine3 lookup(&ctx, &info, &log);
    lookup.setServerName(addressingServerHost);
    lookup.setServerPort(addressingServerPort);

    ConferenceBridge confBridge(&ctx, &info, &log);
    Conference conf(&lookup, &confBridge, &log);
    conf.setCallSign(ourCallSign);
    conf.setFullName(ourFullName);
    conf.setLocation(ourLocation);

    confBridge.setConference(&conf);
    lookup.setConference(&conf);
    ctx.addEventSink(&lm);
    ctx.addEventSink(&lookup);
    ctx.addEventSink(&confBridge);

    // Last thing before going into the event loop
	//watchdog_enable(WATCHDOG_DELAY_MS, true);

    log.info("Entering event loop");

    while (true) {

        // Keep things alive
        watchdog_update();

        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();

        ctx.run();
        lm.run();
        lookup.run();
        confBridge.run();
        conf.run();

        // ----- Serial Commands ---------------------------------------------
        
        int c = getchar_timeout_us(0);
        if (c > 0) {
            if (c == 'q') {
                break;
            } 
            else if (c == 'd') {
                conf.dropAll();
            } 
        }
    }    

    cout << "Out of loop" << endl;

    while (true) {
        // Keep things alive
        watchdog_update();
    }

    return 0;
}
