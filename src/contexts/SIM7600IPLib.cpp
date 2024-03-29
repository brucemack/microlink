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

#include "microtunnel/common.h"

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

void SIM7600IPLib::_write(const uint8_t* data, uint32_t dataLen) {
    cout << "----- Sending -----------------------" << endl;
    prettyHexDump(data, dataLen, cout);
    _uart->write(data, dataLen);
    cout << "-------------------------------------" << endl;
}

void SIM7600IPLib::_write(const char* cmd) {
    _write((const uint8_t*)cmd, strlen(cmd));
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

        // Process each character, attempting to form complete lines
        for (uint32_t i = 0; i < bufLen; i++) {

            if (_rxHoldLen == _rxHoldSize) {
                _log->error("Input oveflow");
            } else {
                _rxHold[_rxHoldLen++] = buf[i];
            }

            if (_inIpd) {
                if (_rxHoldLen == _ipdLen) {
                    _processIPD(_rxHold, _rxHoldLen);
                    _inIpd = false;
                    _rxHoldLen = 0;                
                }
            }
            // Look for the > prompted (used to indicate ready-for-send).
            // Here we are cheating a bit and pretending that there is 
            // a \r\n after the >.
            else if (_rxHoldLen == 1 && _rxHold[0] == '>') {
                _processLine(">", 1);
                _rxHoldLen = 0;
            }
            // Look for a complete line
            else if (_rxHoldLen >= 2 && 
                _rxHold[_rxHoldLen - 2] == 0x0d &&
                _rxHold[_rxHoldLen - 1] == 0x0a) {

                // Prune off the EOL before processing
                _rxHold[_rxHoldLen - 2] = 0;
    
                // Ignore blank lines
                if (_rxHold[0] != 0)
                    _processLine((const char*)_rxHold, _rxHoldLen - 2);

                _rxHoldLen = 0;
            }
        }
    }

    // Check for pending sends
    if (_state == State::RUN) {
        // Anything in the send queue?
        if (_sendQueueWrPtr != _sendQueueRdPtr) {
            // Form the write command
            char buf[64];
            snprintf(buf, 64, "AT+CIPSEND=0,%lu\r\n", _sendQueue[_sendQueueRdPtr].dataLen);
            _write(buf);
            _state = State::SEND_1;
        }
    }

    // Do any other state maintenance
    else if (_state == State::INIT_0) {
        _write("ATE0\r\n");
        _state = State::INIT_1;
    }
    else if (_state == State::INIT_2) {
        _write("AT+NETCLOSE\r\n");
        _state = State::INIT_3;
    }
    else if (_state == State::INIT_4) {
        _write("AT+NETOPEN\r\n");
        _state = State::INIT_5;
    }
    else if (_state == State::INIT_6) {
        _write("AT+CIPOPEN=0,\"TCP\",\"monitor.w1tkz.net\",8100\r\n");
        _state = State::INIT_7;
    }

    return true;
}

void SIM7600IPLib::_processLine(const char* data, uint32_t dataLen) {

    cout << "Processing line: (" << _state << ") " << data << endl;

    // Always look for +IPDnn - asynchronous receive
    if (dataLen >= 5 &&
        data[0] == '+' && data[1] == 'I' && data[2] == 'P' && data[3] == 'D') {
        // Parse length
        _ipdLen = atoi(data + 4);
        _inIpd = true;
    }
    // Waiting for ATE0 be be processed
    else if (_state == State::INIT_1) {
        if (strcmp(data, "OK") == 0) {
            _state = State::INIT_2;
        } 
    }
    // Waiting for AT+NETCLOSE to be processed (will either succeed or fail)
    else if (_state == State::INIT_3) {
        if (strcmp(data, "OK") == 0) {
            _state = State::INIT_3a;
        } 
        // We just ignore the ERROR here
        else if (strcmp(data, "ERROR") == 0) {
            _state = State::INIT_4;
        } 
    }
    // Waiting for the successful AT+NETCLOSE to finish and report status
    else if (_state == State::INIT_3a) {
        if (strcmp(data, "+NETCLOSE: 0") == 0) {
            _state = State::INIT_4;
        } 
    }
    // Waiting for the AT+NETOPEN to be processed
    else if (_state == State::INIT_5) {
        if (strcmp((const char*)data, "OK") == 0) {
            _state = State::INIT_5a;
        } 
        else if (strcmp((const char*)data, "ERROR") == 0) {
            _state = State::FAILED;
        }
    }
    // Waiting for the successful AT+NETOPEN to finish and report status
    else if (_state == State::INIT_5a) {
        if (strcmp((const char*)data, "+NETOPEN: 0") == 0) {
            _state = State::INIT_6;
        } 
    }
    // Waiting for the AT+CIPOPEN to be processed
    else if (_state == State::INIT_7) {
        if (strcmp((const char*)data, "OK") == 0) {
            _state = State::INIT_7a;
        } 
        else if (strcmp((const char*)data, "ERROR") == 0) {
            _state = State::FAILED;
        }
    }
    // Waiting for the successful AT+CIPOPEN
    else if (_state == State::INIT_7a) {
        if (strcmp((const char*)data, "+CIPOPEN: 0,0") == 0) {
            _state = State::RUN;
        } 
    }
    else if (_state == State::SEND_1) {
        if (strcmp(data, ">") == 0) {
            _write(_sendQueue[_sendQueueRdPtr].data, _sendQueue[_sendQueueRdPtr].dataLen);
            _state = State::SEND_2;
        }
    }
    // Waiting for an AT+CIPSEND to be acknowledged
    else if (_state == State::SEND_2) {
        if (strcmp(data, "OK") == 0) {
            _state = State::SEND_3;
        }
    }
    else if (_state == State::SEND_3) {

        // Form the send reponse we are waiting for
        char buf[64];
        snprintf(buf, 64, "+CIPSEND: 0,%lu,%lu", 
            _sendQueue[_sendQueueRdPtr].dataLen,
            _sendQueue[_sendQueueRdPtr].dataLen);

        if (strcmp(data, buf) == 0) {
            // Pop the send queue
            _sendQueueRdPtr++;
            // Deal with wrap
            if (_sendQueueRdPtr == _sendQueueSize) {
                _sendQueueRdPtr = 0;
            }
            _state = State::RUN;
        }
    }
}

void SIM7600IPLib::_processIPD(const uint8_t* data, uint32_t dataLen) {

    if (_ipdHoldLen + dataLen > _ipdHoldSize) {
        _log->error("IPD overflow");
        return;
    }

    // Append accumulated data
    memcpy(_ipdHold + _ipdHoldLen, data, dataLen);
    _ipdHoldLen += dataLen;

    // Check for a complete frame
    if (_ipdHoldLen < 2) {
        return;
    }
    uint16_t frameLen = _ipdHold[0] << 8 | _ipdHold[1];
    if (_ipdHoldLen < frameLen) {
        return;
    }

    // Pull out the frame into a stand-alone buffer
    uint8_t frame[2048];
    memcpy(frame, _ipdHold, frameLen);
    // If there's anything left, shift down to the start of the hold area
    if (_ipdHoldLen > frameLen) {
        for (uint32_t i = 0; i < (_ipdHoldLen - frameLen); i++)
            _ipdHold[i] = _ipdHold[frameLen + 1];
        _ipdHoldLen -= frameLen;
    } else {
        _ipdHoldLen = 0;
    }

    if (frameLen >= 4) {

        cout << "Frame" << endl;
        prettyHexDump(frame, frameLen, cout);

        if (frame[2] == ClientFrameType::RESP_QUERY_DNS) {
            if (frameLen < 9) {
                _log->error("Invalid DNS response");
                return;
            }
            if (frame[3] != 0) {
                return;
            }
            uint32_t hostNameLen = frameLen - 8;
            if (hostNameLen > 63) {
                return;
            }

            uint32_t addr = (frame[4] << 24) | (frame[5] << 16) | (frame[6] << 8) |
                frame[7];
            char hostName[64];
            memcpyLimited((uint8_t*)hostName, frame + 8, hostNameLen, 63);
            hostName[hostNameLen] = 0;

            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->dns(HostName(hostName), IPAddress(addr));
        }
        else if (frame[2] == ClientFrameType::RESP_OPEN_TCP) {
            if (frameLen != 6) {
                _log->error("Invalid response");
                return;
            }
            if (frame[5] != 0) {
                return;
            }
            uint16_t id = frame[3] << 8 | frame[4];
            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->conn(Channel(id));
        }
        else {
            _log->info("Unsupported response type");
        }
    }
}

void SIM7600IPLib::_queueSend(const uint8_t* d, uint32_t dl) {
    _sendQueue[_sendQueueWrPtr] = Send(d, dl);
    _sendQueueWrPtr++;
    // Deal with wrap
    if (_sendQueueWrPtr == _sendQueueSize) {
        _sendQueueWrPtr = 0;
    }
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
}

bool SIM7600IPLib::isLinkUp() const {
    return _state == State::RUN;
}

void SIM7600IPLib::queryDNS(HostName hostName) {

    if (traceLevel > 0)
        _log->info("DNS request for %s", hostName.c_str());

    // Make a packet
    RequestQueryDNS req;
    req.len = sizeof(req);
    req.type = ClientFrameType::REQ_QUERY_DNS;
    strncpy(req.name, hostName.c_str(), 64);
    // Queue for delivery
    _queueSend((const uint8_t*)&req, sizeof(req));
}

Channel SIM7600IPLib::createTCPChannel() {
    return Channel(_channelCount++, true);
}

void SIM7600IPLib::closeChannel(Channel c) {
}

void SIM7600IPLib::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {

    char addrStr[20];
    formatIP4Address(ipAddr.getAddr(), addrStr, 20);

    if (traceLevel > 0)
        _log->info("Connecting %d to %s:%d", c.getId(), addrStr, port);
    
    // Make a packet
    RequestOpenTCP req;
    req.len = sizeof(req);
    req.type = ClientFrameType::REQ_OPEN_TCP;
    req.clientId = c.getId();
    req.addr = ipAddr.getAddr();
    req.port = (uint16_t)port;
    _queueSend((const uint8_t*)&req, sizeof(req));
}

void SIM7600IPLib::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {    
    if (traceLevel > 0)
        _log->info("Sending %d", c.getId());
    
    RequestSendTCP req;
    req.len = len + 6;
    req.type = ClientFrameType::REQ_SEND_TCP;
    req.clientId = c.getId();
    memcpyLimited(req.contentPlaceholder, b, len, 2048 - 6);
    // Queue for delivery
    _queueSend((const uint8_t*)&req, req.len);
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
