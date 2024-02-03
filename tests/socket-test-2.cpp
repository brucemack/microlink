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

/*
Building w/ debug enabled (needed for asserts)
cmake -DCMAKE_BUILD_TYPE=Debug ..

Load command:
openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program socket-test-2.elf verify reset exit"

Minicom (for console, not the UART being tested):
minicom -b 115200 -o -D /dev/ttyACM0
*/

#include <iostream>
#include <cassert>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/EventProcessor.h"
#include "kc1fsz-tools/rp2040/PicoUartChannel.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"

#include "common.h"
#include "contexts/ESP32CommContext.h"

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

class TestEventProcessor : public EventProcessor {
public:

    int dnsCount = 0;
    int udpCount = 0;

    virtual void processEvent(const Event* ev) {

        cout << "EVENT TYPE " << ev->getType() << endl;

        if (ev->getType() == DNSLookupEvent::TYPE) {
            const DNSLookupEvent* evt = (const DNSLookupEvent*)ev;
            char buf[64];
            formatIP4Address(evt->getAddr().getAddr(), buf, 64);
            cout << "  Got address: " << buf << endl;
            dnsCount++;
        }
        else if (ev->getType() == UDPReceiveEvent::TYPE) {
            const UDPReceiveEvent* evt = (const UDPReceiveEvent*)ev;
            cout << "  Got UDP Data: [";
            cout.write((const char*)evt->getData(), evt->getDataLen());
            cout << "]" << endl;
            udpCount++;
        }
        else if (ev->getType() == TCPConnectEvent::TYPE) {
            const TCPConnectEvent* evt = (const TCPConnectEvent*)ev;
            cout << "  Got connect: " << evt->getChannel().getId() << endl;
        }
    }
};

int main(int,const char**) {

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

    cout << "Socket Test 2" << endl;

    // Sertup UART and timer
    const uint32_t readBufferSize = 256;
    uint8_t readBuffer[readBufferSize];
    const uint32_t writeBufferSize = 256;
    uint8_t writeBuffer[writeBufferSize];

    PicoUartChannel channel(UART_ID, 
        readBuffer, readBufferSize, writeBuffer, writeBufferSize);

    cout << "A" << endl;

    PicoPollTimer timer;
    timer.setIntervalUs(1000 * 5000);

    ESP32CommContext ctx(&channel);
    TestEventProcessor evp;
    ctx.setEventProcessor(&evp);

    const char* cmd;
    uint32_t cmdLen;

    // TODO: MAKE A NICE WAY TO STREAM A SET OF INITIAL COMMANDS

    // Stop echo
    cmd = "ATE0\r\n";
    cmdLen = strlen(cmd);
    channel.write((const uint8_t*)cmd, cmdLen);
    sleep_ms(10);

    // Setup station mode
    cmd = "AT+CWMODE=1\r\n";
    cmdLen = strlen(cmd);
    channel.write((const uint8_t*)cmd, cmdLen);
    sleep_ms(10);

    // Setup mux
    cmd = "AT+CIPMUX=1\r\n";
    cmdLen = strlen(cmd);
    channel.write((const uint8_t*)cmd, cmdLen);
    sleep_ms(10);

    // Close all connections
    cmd = "AT+CIPCLOSE=5\r\n";
    cmdLen = strlen(cmd);
    channel.write((const uint8_t*)cmd, cmdLen);
    sleep_ms(10);

    // Try getting a DNS resolution        
    //ctx.startDNSLookup(HostName("www.google.com"));

    // Try opening a TCP socket
    //Channel c = ctx.createTCPChannel();
    //IPAddress addr(parseIP4Address("142.251.40.164"));
    //ctx.connectTCPChannel(c, addr, 80);

    // Send some data on the socket
    //ctx.sendTCPChannel(c, (uint8_t*)"X\n", 2);

    while (true) {        
        ctx.poll();
    }

    return 0;
}
