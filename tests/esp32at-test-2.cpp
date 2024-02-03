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
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"

#include "ATResponseProcessor.h"

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

/*
Building w/ debug enabled (needed for asserts)
cmake -DCMAKE_BUILD_TYPE=Debug ..

Load command:
openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program esp32at-test-2.elf verify reset exit"

Minicom (for console, not the UART being tested):
minicom -b 115200 -o -D /dev/ttyACM0
*/

bool runCmd(PicoUartChannel& channel, 
    const char* cmd, const char* respToken, 
    uint32_t to, uint8_t* preText, uint32_t preTextSize, uint32_t* preTextLen) {
    // Send the command
    if (channel.write((const uint8_t*)cmd, strlen(cmd)) != (int32_t)strlen(cmd)) {
        return false;
    }
    // Wait for result
    bool b = waitOnCompletion(channel, respToken, to, preText, preTextSize, preTextLen);
    return b;
}

class TestSink : public ATResponseProcessor::EventSink {
public:
    virtual void ok() {
        cout << "OK" << endl;
        result0 = 1;
    }
    virtual void sendOk() {
        cout << "SEND OK" << endl;
        result0 = 2;
    }
    virtual void error()  {
        cout << "ERROR" << endl;
        result0 = 3;
    }
    virtual void sendPrompt()  {
        cout << "SEND PROMPT" << endl;
        result0 = 4;
    }
    virtual void sendSize()  {
        cout << "SEND SIZE" << endl;
        result0 = 5;
    }
    virtual void domain(const char* addr)  {
        cout << "Domain: " << addr << endl;
        result0 = 12;
    }
    virtual void ipd(uint32_t channel, uint32_t chunk,
        const uint8_t* data, uint32_t len)  {

        cout << "IPD " << channel << ", " << chunk << ", " << len << endl;
        prettyHexDump(data, len, cout);

        result0 = 6;
        result1 = channel;
    }
    virtual void connected(uint32_t channel)  {
        cout << "CONNECTED " << channel << endl;
        result0 = 11;
        result1 = channel;
    }
    virtual void closed(uint32_t channel)  {
        cout << "CLOSED " << channel << endl;
        result0 = 7;
        result1 = channel;
    }
    virtual void notification(const uint8_t* data, uint32_t len)  {
        cout << "NOTIFICATION: ";
        cout.write((const char*)data, len);
        cout << endl;
        result0 = 8;
    }
    virtual void  confused(const uint8_t* data, uint32_t len)  {
        cout << "CONFUSED" << endl;
        prettyHexDump(data, len, cout);
        result0 = 99;
    }

    uint32_t result0, result1;
};

static void test_0() {

    cout << "Unit tests 0 starting" << endl;

    TestSink sink;
    ATResponseProcessor p(&sink);    

    const char* s = "\r\nOK\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 1);

    s = "\r\nSEND OK\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 2);

    s = "\r\nERROR\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 3);

    s = "\r\n>";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 4);

    // NOTE: Content length doesn't include the trailing \r\n
    s = "\r\n+IPD,1,5:henry\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 6);
    assert(sink.result1 == 1);

    s = "\r\n+CIPDOMAIN:\"1.2.3.4\"\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 12);

    s = "2,CLOSED\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 7);
    assert(sink.result1 == 2);

    s = "0,CONNECT\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 11);
    assert(sink.result1 == 0);

    s = "WIFI DISCONNECTED\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 8);

    // Demonstrate abilty to hold parse state across calls
    sink.result0 = 0;
    sink.result1 = 0;
    s = "2,CL";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 0);
    assert(sink.result1 == 0);
    s = "OSED\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 7);
    assert(sink.result1 == 2);

    s = "\r\nUNEXPECTED TOKEN\r\n";
    p.process((const uint8_t*)s, strlen(s));
    assert(sink.result0 == 99);

    cout << "All Tests Passed!" << endl;
}

static int test_1() {
 
    cout << "Hello ESP32-AT 2" << endl;
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

    const uint32_t preTextSize = 256 * 16;
    uint8_t preText[preTextSize];
    uint32_t preTextLen = 0;
    uint32_t to = 0;

    // RESET
    //runCmd(channel, "AT+RST\r\n",
    //    "\r\nready", to, preText, preTextSize, &preTextLen);
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

    // Setup TCP connection
    //runCmd(channel, "AT+CIPSTART=1,\"TCP\",\"142.250.176.196\",80\r\n",
    //    "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);
    // EchoLink Server
    runCmd(channel, "AT+CIPSTART=1,\"TCP\",\"129.213.119.249\",5200\r\n",
        "\r\nOK\r\n", to, preText, preTextSize, &preTextLen);

    const char* frame = 
        "lKC1FSZ\254\254echolink667\rONLINE0.02MLZ(20:31)\rWellesley, MA USA\r";
    uint32_t frameLen = strlen(frame);

    char cmdBuf[64];
    sprintf(cmdBuf, "AT+CIPSEND=1,%d\r\n", frameLen);
    //const char* cmd = "AT+CIPSEND=1,2\r\n";
    //channel.write((uint8_t*)cmd, strlen(cmd));    
    channel.write((uint8_t*)cmdBuf, strlen(cmdBuf));

    // TEMP
    sleep_ms(5);
    channel.write((uint8_t*)frame, frameLen);

    PicoPollTimer timer2;
    timer2.setIntervalUs(250 * 1000);
    int i = 0;

    TestSink sink;
    ATResponseProcessor proc(&sink);    

    while (true) {

        // Display whatever comes in
        channel.poll();       

        //if (timer2.poll()) {
        //    cout << i++ << endl;
        //    cout << "==============" << endl;
        //    cmd = "AT\r\n";
        //    channel.write((uint8_t*)cmd, strlen(cmd));
        //}

        if (channel.isReadable()) {
            uint8_t buf[256];
            int len = channel.read(buf, 256);
            // Feed into the AT processor
            proc.process(buf, len);
        }  
    }
}

int main(int, const char**) {

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

   test_0();
   test_1();

    while (true) {        
    }
}
