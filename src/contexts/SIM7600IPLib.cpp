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
 */
#include <cassert>
#include <iostream>
#include <algorithm>

#include "pico/time.h"
                                 
#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/AsyncChannel.h"

#include "../common.h"
#include "../BooleanHolder.h"

#include "SIM7600IPLib.h"

using namespace std;

namespace kc1fsz {

int SIM7600IPLib::traceLevel = 0;

SIM7600IPLib::SIM7600IPLib(Log* log, AsyncChannel* uart) 
:   _log(log),
    _uart(uart),
    _state(State::IDLE) {
}

// ----- Runnable Methods ------------------------------------------------

/**
    * This should be called from the event loop.  It attempts to make forward
    * progress and passes all events to the event processor.
    * 
    * @returns true if any events were dispatched.
*/
bool SIM7600IPLib::run() {
    
    bool anythingHappened = _uart->run();

    if (_uart->isReadable()) {

        // Pull what we can off the UART
        const uint32_t bufSize = 256;
        uint8_t buf[bufSize];
        uint32_t bufLen = _uart->read(buf, bufSize);
        prettyHexDump(buf, bufLen, cout);

        for (uint32_t i = 0; i < bufLen; i++) {
            _rxHold[_rxHoldLen++] = buf[i];
            if (_rxHoldLen >= 2) {
                if (_rxHold[_rxHoldLen - 2] == 0x0d &&
                    _rxHold[_rxHoldLen - 1] == 0x0a) {
                    // Prune off the EOL before processing
                    _rxHold[_rxHoldLen - 2] = 0;
                    cout << _state << " Got line: [" << _rxHold << "]" << endl;
                    _rxHoldLen = 0;

                    if (_state == State::INIT_0) {
                        if (strcmp((const char*)_rxHold, "OK") == 0) {
                            _state = State::INIT_1;
                            const char* cmd = "ATE0\r\n";
                            uint32_t cmdLen = strlen(cmd);
                            _uart->write((uint8_t*)cmd, cmdLen);
                        }
                    }
                    else if (_state == State::INIT_1) {
                        if (strcmp((const char*)_rxHold, "OK") == 0) {
                            _state = State::INIT_2;
                            const char* cmd = "AT+NETOPEN?\r\n";
                            uint32_t cmdLen = strlen(cmd);
                            _uart->write((uint8_t*)cmd, cmdLen);
                        }
                    }
                    else if (_state == State::INIT_2) {
                        // Look for the case where the network is already open
                        if (strcmp((const char*)_rxHold, "+NETOPEN: 1") == 0) {
                            _isNetOpen = true;
                        } 
                        else if (strcmp((const char*)_rxHold, "+NETOPEN: 0") == 0) {
                            _isNetOpen = false;
                        }
                        else if (strcmp((const char*)_rxHold, "OK") == 0) {
                            if (_isNetOpen) {
                                _state = State::INIT_3;
                                const char* cmd = "AT+NETCLOSE\r\n";
                                uint32_t cmdLen = strlen(cmd);
                                _uart->write((uint8_t*)cmd, cmdLen);
                            } else {
                                _state = State::INIT_5;
                                const char* cmd = "AT+NETOPEN\r\n";
                                uint32_t cmdLen = strlen(cmd);
                                _uart->write((uint8_t*)cmd, cmdLen);
                            }
                        }
                    }
                    // After a close we get an OK and then a status
                    else if (_state == State::INIT_3) {
                        if (strcmp((const char*)_rxHold, "OK") == 0) {
                            _state = State::INIT_4;
                        }
                    }
                    else if (_state == State::INIT_4) {
                        if (strcmp((const char*)_rxHold, "+NETCLOSE: 0") == 0) {
                            _state = State::INIT_5;
                            const char* cmd = "AT+NETOPEN\r\n";
                            uint32_t cmdLen = strlen(cmd);
                            _uart->write((uint8_t*)cmd, cmdLen);
                        }
                    }
                    // After an open we get an OK and then a status
                    else if (_state == State::INIT_5) {
                        if (strcmp((const char*)_rxHold, "OK") == 0) {
                            _state = State::INIT_6;
                        }
                    }
                    else if (_state == State::INIT_6) {
                        if (strcmp((const char*)_rxHold, "+NETOPEN: 0") == 0) {
                            _state = State::INIT_7;
                            const char* cmd = "AT+CIPOPEN=0,\"TCP\",\"monitor.w1tkz.net\",8100\r\n";
                            uint32_t cmdLen = strlen(cmd);
                            _uart->write((uint8_t*)cmd, cmdLen);
                        }
                    }
                    else if (_state == State::INIT_7) {
                        if (strcmp((const char*)_rxHold, "OK") == 0) {
                            _state = State::RUN;
                        }
                    }
                }
            }
        }

        anythingHappened = true;
    }

    return anythingHappened;    
}

void SIM7600IPLib::addEventSink(IPLibEvents* e) {
    if (_eventsLen < _maxEvents) {
        _events[_eventsLen++] = e;
    } else {
        panic_unsupported();
    }
}

void SIM7600IPLib::reset() {

    _state = State::INIT_0;

    const char* cmd = "\r\nAT\r\n";
    uint32_t cmdLen = strlen(cmd);
    _uart->write((uint8_t*)cmd, cmdLen);
}

bool SIM7600IPLib::isLinkUp() const {
    return _state == State::RUN;
}

void SIM7600IPLib::queryDNS(HostName hostName) {
    if (traceLevel > 0)
        _log->info("DNS request for %s", hostName.c_str());

    // Make a packet
    uint8_t buf[64];
    uint16_t totalLen = 3 + strlen(hostName.c_str());
    buf[0] = (totalLen & 0xff00) >> 8;
    buf[1] = (totalLen & 0x00ff);
    buf[2] = 7;
    memcpy(buf + 3, hostName.c_str(), strlen(hostName.c_str()));

    // Build the send command
    char cmd[64];
    snprintf(cmd, 64, "AT+CIPSEND=0,%d\r\n", totalLen);
    _uart->write((uint8_t*)cmd, strlen(cmd));
}

Channel SIM7600IPLib::createTCPChannel() {
    return Channel(0, false);
}

void SIM7600IPLib::closeChannel(Channel c) {
}

void SIM7600IPLib::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {

    char addrStr[20];
    formatIP4Address(ipAddr.getAddr(), addrStr, 20);

    if (traceLevel > 0)
        _log->info("Connecting to %s:%d", addrStr, port);
}

void SIM7600IPLib::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {    
}

Channel SIM7600IPLib::createUDPChannel() {
    return Channel(0, false);
}

void SIM7600IPLib::bindUDPChannel(Channel c, uint32_t localPort) {
    if (traceLevel > 0)
        _log->info("Binding channel %d to port %d", c.getId(), localPort);
}

void SIM7600IPLib::sendUDPChannel(const Channel& c, 
    const IPAddress& remoteIpAddr, uint32_t remotePort,
    const uint8_t* b, uint16_t len) {
}

}
