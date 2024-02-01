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
#include <stdio.h>
#include <iostream>
#include <cctype>
#include <cstring>
#include <string>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/rp2040/PicoUartChannel.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"

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

static const uint32_t accSize = 256;
uint8_t acc[accSize];
uint32_t accLen = 0;

static constexpr const char* ERROR_TOKEN = "\r\nERROR\r\n";
static constexpr int ERROR_TOKEN_LEN = std::char_traits<char>::length(ERROR_TOKEN);

/**
 * A function that is helpful when dealing with AT+ command protocols.
 * Locates either the token specified or \r\nERROR\r\n and returns its 
 * starting position in the accumulator provided.
 *
 * @param acc
 * @param accLen
 * @param loc This is where the location of the start of the 
 *   token is located.
 * @returns true if something was found, or false if nothing was found.
 *  
 */
bool findToken(const uint8_t* acc, uint32_t accLen, const char* token, 
    uint32_t* loc, uint32_t* len) {

    // Check for the target token
    const int tokenLen = strlen(token);

    for (int i = 0; i < accLen; i++) {

        int matchLen = 0;

        for (int k = 0; k < tokenLen && i + k < accLen; k++) {
            if (acc[i + k] == token[k]) {
                matchLen++;
            } else {
                break;
            }
        }
        // Did we match an entire term?
        if (matchLen == tokenLen) {
            *loc = i;
            *len = tokenLen;
            return true;
        }

        matchLen = 0;
        for (int k = 0; k < ERROR_TOKEN_LEN && i + k < accLen; k++) {
            if (acc[i + k] == ERROR_TOKEN[k]) {
                matchLen++;
            } else {
                break;
            }
        }
        // Did we match an entire term?
        if (matchLen == ERROR_TOKEN_LEN) {
            *loc = i;
            *len = ERROR_TOKEN_LEN;
            return true;
        }
    }

    return false;
}

/**
 * @returns true on success, false on ERROR
 */
bool waitOn(PicoUartChannel& channel, const char* token, uint32_t timeOut,
    uint8_t* preText, uint32_t preTextSize, uint32_t* preTextLen) {

    while (true) {

        // Since we're in a blocking loop here, prompt the channel to 
        // make sure we're making forward progress in all the right ways.        
        channel.poll();

        if (channel.isReadable()) {
            
            // Read directly into the accumulator
            uint32_t accFree = accSize - accLen;
            accLen += channel.read(acc + accLen, accFree);

            //cout << "ACC: " << accLen << endl;
            //prettyHexDump(acc, accLen, cout);

            // Check for termination
            uint32_t tokenLoc = 0;
            uint32_t tokenLen = 0;
            bool b = findToken(acc, accLen, token, &tokenLoc, &tokenLen);
            if (b) {
                // Copy the pre-text (if any)
                if (tokenLoc > 0) {
                    for (unsigned int i = 0; 
                        i < preTextSize && i < tokenLoc; i++) {
                            preText[i] = acc[i];
                    }
                    *preTextLen = tokenLoc;
                }
                // Failure is when the ERROR token is found
                bool ret = acc[tokenLoc + 2] != 'E';
                // Flush the accumulator
                accLen = 0;
                return ret;
            }
        }
    }
    return false;
}

bool runCmd(PicoUartChannel& channel, 
    const char* cmd, const char* respToken, 
    uint32_t to, uint8_t* preText, uint32_t preTextSize, uint32_t* preTextLen) {
    // Send the command
    if (channel.write((const uint8_t*)cmd, strlen(cmd)) != (int32_t)strlen(cmd)) {
        return false;
    }
    // Wait for result
    bool b = waitOn(channel, respToken, to, preText, preTextSize, preTextLen);
    return b;
}

/*
Load command:
openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program esp32at-test-1.elf verify reset exit"

Minicom (for console, not the UART being tested):
minicom -b 115200 -o -D /dev/ttyACM0
*/
int main() {
 
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

    cout << "Hello ESP32-AT 3" << endl;
    cout << endl;

    // Sertup UART and timer
    const uint32_t readBufferSize = 256;
    uint8_t readBuffer[readBufferSize];
    const uint32_t writeBufferSize = 256;
    uint8_t writeBuffer[writeBufferSize];

    PicoUartChannel channel(UART_ID, 
        readBuffer, readBufferSize, writeBuffer, writeBufferSize);

    PicoPollTimer timer;
    timer.setIntervalUs(1000 * 5000);

    uint8_t preText[256];
    const uint32_t preTextSize = 256;
    uint32_t preTextLen = 0;
    uint32_t to = 0;

    // RESET
    //uart_puts(UART_ID, "AT+RST\r\n");
    //waitResponse(UART_ID, "\r\nready", 0);

    // Stop echo
    runCmd(channel, "ATE0\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
    // Display state
    cout << "Second command" << endl;
    runCmd(channel, "AT+CIPSTATE?\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
    // Setup station mode
    runCmd(channel, "AT+CWMODE=1\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
    // Setup mux
    runCmd(channel, "AT+CIPMUX=1\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);        
    // Close all connections
    runCmd(channel, "AT+CIPCLOSE=5\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
    preText[preTextLen] = 0;
    cout << "PRE [" << preText << "] len " << preTextLen << endl;
    // Setup UDP receive
    cout << "Receive 1" << endl;
    runCmd(channel, "AT+CIPSTART=0,\"UDP\",\"192.168.8.102\",5198,5198,0\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
    absolute_time_t start = get_absolute_time();

    for (int j = 0; j < 10; j++) {

        cout << j << endl;

        uint8_t frame[144];
        for (int i = 0; i < 144; i++) {
            frame[i] = 'a' + j;
        }
        absolute_time_t start2 = get_absolute_time();
        // Do a send of 144
        runCmd(channel, "AT+CIPSEND=0,144,\"192.168.8.102\",5198\r\n",
            "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
        absolute_time_t end2 = get_absolute_time();
        cout << "  Big Cmd " << absolute_time_diff_us(start2, end2) << endl;

        absolute_time_t start3 = get_absolute_time();
        channel.blockAndFlush(0);
        channel.write(frame, 144);
        absolute_time_t end3 = get_absolute_time();
        cout << "  Big Write " << absolute_time_diff_us(start3, end3) << endl;

        absolute_time_t start4 = get_absolute_time();
        waitOn(channel, "\r\nSEND OK\r\n", to, preText, preTextSize, &preTextLen);
        absolute_time_t end4 = get_absolute_time();
        cout << "  Big Wait " << absolute_time_diff_us(start4, end4) << endl;
    }
    absolute_time_t end = get_absolute_time();
    cout << "Elapsed " << absolute_time_diff_us(start, end) << endl;

    // We always have one garbage charcter (10?) on the UART at
    // startup.

    while (true) {

        // Display whatever comes in
        channel.poll();

        // Check for result
        if (channel.isReadable()) {
            uint8_t buf[256];
            int len = channel.read(buf, 256);
            cout.write((const char*)buf, len);
            cout.flush();
        }
    }
}

