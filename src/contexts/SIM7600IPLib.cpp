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

// Delay between attempts to open the connection to the tunnel
#define OPEN_INTERVAL_MS (2000)

using namespace std;

namespace kc1fsz {

int SIM7600IPLib::traceLevel = 0;

SIM7600IPLib::SIM7600IPLib(Log* log, AsyncChannel* uart, uint32_t resetPin) 
:   _log(log),
    _uart(uart),
    _resetPin(resetPin),
    _sendQueuePtr(_sendQueueSize) {
}

void SIM7600IPLib::_write(const uint8_t* data, uint32_t dataLen) {
    //_log->debugDump("To UART", data, dataLen);
    _uart->write(data, dataLen);
}

void SIM7600IPLib::_write(const char* cmd) {
    _write((const uint8_t*)cmd, strlen(cmd));
}

// ----- Runnable Methods ------------------------------------------------

/**
 * This should be called from the event loop.  It attempts to make forward
 * progress and passes all events to the event processor.
*/
void SIM7600IPLib::run() {
    
    _uart->run();

    if (_uart->isReadable()) {

        // Pull what we can off the UART
        const uint32_t bufSize = 256;
        uint8_t buf[bufSize];
        uint32_t bufLen = _uart->read(buf, bufSize);

        //_log->debugDump("From UART", buf, bufLen);

        // Process each character, attempting to form complete lines
        for (uint32_t i = 0; i < bufLen; i++) {

            if (_rxHoldLen == _rxHoldSize) {
                _log->error("Input overflow");
            } else {
                _rxHold[_rxHoldLen++] = buf[i];
            }

            if (_inIpd) {
                // Look for the completion of the IPD accumulation
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

    // Check to see if we've accumulated a complete frame from the 
    // proxy tunnel.  If so, handle it.
    _processProxyFrameIfPossible();

    // The first thing we do is force some junk output in case the 
    // module was preveiously stuck at a > prompt. The junk ends
    // with a \r\n so it should generate an ERROR under normal 
    // circumstances.
    if (_state == State::INIT_0a) {
        char junk[128];
        for (uint8_t i = 0; i < 128; i++)
            junk[i] = 'x';
        junk[125] = '\r';
        junk[126] = '\n';
        junk[127] = 0;
        _write(junk);
        _state = State::INIT_0b;
        _stateTime = time_ms();
    }
    // We wait for a few ms and then request a reset
    else if (_state == State::INIT_0b) {
        if (ms_since(_stateTime) > 100) {
            _write("AT+CRESET\r\n");
            _state = State::INIT_0c;
        }
    }
    // Here we are waiting for the need to do a more serious
    // reset.
    else if (_state == State::INIT_0c) {
    }
    // Turn off echo
    else if (_state == State::INIT_0) {
        _write("ATE0\r\n");
        _state = State::INIT_1;
    }
    else if (_state == State::INIT_4) {
        _log->info("Opening TCP network");
        _write("AT+NETOPEN\r\n");
        _state = State::INIT_5;
    }
    else if (_state == State::INIT_CSQ_0) {
        _write("AT+CSQ\r\n");
        _state = State::INIT_CSQ_1;
    }
    // Delay before attempting to open the connection
    else if (_state == State::INIT_6h) {
        if (ms_since(_stateTime) > OPEN_INTERVAL_MS) {
            _state = State::INIT_6;
        }
    }
    else if (_state == State::INIT_6) {
        _log->info("Opening tunnel");
        _write("AT+CIPOPEN=0,\"TCP\",\"monitor.w1tkz.net\",8100\r\n");
        _state = State::INIT_7;
    }
    else if (_state == State::RECON_0) {
        _log->info("Closing TCP network");
        _write("AT+NETCLOSE\r\n");
        _state = State::RECON_1;
    }
    // Check for pending sends
    else if (_state == State::RUN) {
        // Anything in the send queue?
        if (!_sendQueuePtr.isEmpty()) {
            // Pop the item off the queue immediately to avoid issues
            // with overruns
            _workingSend = _sendQueue[_sendQueuePtr.getAndIncReadPtr()];
            // Form the send command based on the length
            char buf[64];
            snprintf(buf, 64, "AT+CIPSEND=0,%lu\r\n", _workingSend.dataLen);
            _write(buf);
            // In this state we wait for the prompt
            _state = State::SEND_1;
        }
    }
}

static bool streq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

void SIM7600IPLib::_processLine(const char* data, uint32_t dataLen) {

    if (_state == State::RUN) {
        // Always look for +IPDnn - asynchronous receive
        if (dataLen >= 5 &&
            data[0] == '+' && data[1] == 'I' && data[2] == 'P' && data[3] == 'D') {
            // Parse length
            _ipdLen = atoi(data + 4);
            _inIpd = true;
        }
        // Look for the source IP address
        else if (dataLen > 10 && memcmp(data, "RECV FROM:", 10) == 0) {
            auto [addr, port] = parseAddressAndPort(data + 10);
            _lastAddr = addr;
            _lastPort = port;
        }
        // Look for the unsolicited tunnel disconnect
        else if (streq(data, "+IPCLOSE: 0,1")) {
            _log->info("Tunnel dropped");
            // Go to close the network
            _state = State::RECON_0;
        }
        // Look for the unsolicited network drop
        else if (streq(data, "+CIPEVENT: NETWORK CLOSED UNEXPECTEDLY")) {
            _log->info("Network dropped");
            // Go to the beginning
            _state = State::INIT_0a;
        }
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    } 
    // Waiting during startup
    else if (_state == State::INIT_0c) {
        if (streq(data, "PB DONE")) {
            _log->info("Cellular module initialized");
            _state = State::INIT_0;
        } 
    }
    // Waiting for ATE0 be be processed
    else if (_state == State::INIT_1) {
        if (streq(data, "OK")) {
            _state = State::INIT_4;
        } 
        else if (streq(data, "ATE0")) {
            // Ignore the echo of this command
        } 
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    // Waiting for the AT+NETOPEN to be processed
    else if (_state == State::INIT_5) {
        if (streq(data, "OK")) {
            _state = State::INIT_5a;
        } 
        else if (strcmp((const char*)data, "ERROR") == 0) {
            _state = State::FAILED;
        }
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    // Waiting for the successful AT+NETOPEN to finish and report status
    else if (_state == State::INIT_5a) {
        if (streq(data, "+NETOPEN: 0")) {
            _state = State::INIT_CSQ_0;
        } 
    }
    // Waiting for the successful AT+CSQ to finish
    else if (_state == State::INIT_CSQ_1) {
        if (streq(data, "OK") == 0) {
            _state = State::INIT_6;
        } 
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
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
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    // Waiting for the successful AT+CIPOPEN
    else if (_state == State::INIT_7a) {
        if (streq(data, "OK")) {
            // Ignore this
        } 
        else if (streq(data, "+CIPOPEN: 0,0")) {
            _state = State::RUN;
            // Let everyone know that we've reset
            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->reset();
        } 
        // Look for the case where we fail to connect (tunnel is 
        // not available)
        else if (streq(data, "+CIPOPEN: 0,1")) {
            // Delay before retry
            _state = State::INIT_6h;
            _stateTime = time_ms();
        }
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    else if (_state == State::SEND_1) {
        if (streq(data, ">")) {
            _write(_workingSend.data, _workingSend.dataLen);
            // Wait for the OK that the send is fully complete
            _state = State::SEND_2;
        }
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    // Waiting for an AT+CIPSEND to be acknowledged
    else if (_state == State::SEND_2) {
        if (streq(data, "OK")) {
            _state = State::SEND_3;
        }
        // NOTE: We've discovered that +IPDnnn can come in during the 
        // wait for an OK
        else if (dataLen >= 5 &&
            data[0] == '+' && data[1] == 'I' && data[2] == 'P' && data[3] == 'D') {
            // Parse length
            _ipdLen = atoi(data + 4);
            _inIpd = true;
        }
        // NOTE: We've discovered that RECV FROM: can come in during the 
        // wait for an OK
        else if (dataLen > 10 && memcmp(data, "RECV FROM:", 10) == 0) {
            auto [addr, port] = parseAddressAndPort(data + 10);
            _lastAddr = addr;
            _lastPort = port;
        }
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    else if (_state == State::SEND_3) {

        // Form the send reponse we are waiting for
        char buf[64];
        snprintf(buf, 64, "+CIPSEND: 0,%lu,%lu", 
            _workingSend.dataLen, _workingSend.dataLen);
        // TODO: What do do if there is no match?
        if (streq(data, buf)) {
            _state = State::RUN;
        }
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
    // Waiting for the AT+NETCLOSE to be processed
    else if (_state == State::RECON_1) {
        if (streq(data, "+NETCLOSE: 0")) {
            _state = State::INIT_4;
        } 
        else {
            _log->info("Unexpected line: %s", data);
            _log->info("In state %d", _state);
        }
    }
}

/*
This is a bit tricky because the +IPD messages are just streaming 
data in from the proxy without and regard for the alignment of 
the proxy frames.  We accumulate the bytes that we receive from
the proxy in _ipdHold (w/ _ipdHoldLen) and watch until we have 
accumulated a complete proxy frame.

Once a full frame has been received we process it and then 
"shift down" any remaining bytes since they make up the next
proxy frame.  We don't want to loose the next one!
*/
void SIM7600IPLib::_processIPD(const uint8_t* data, uint32_t dataLen) {

    if (_ipdHoldLen + dataLen > _ipdHoldSize) {
        _log->error("IPD overflow");
        return;
    }

    // Append received data to the accumulator in an attempt to form a 
    // complete proxy frame.
    memcpy(_ipdHold + _ipdHoldLen, data, dataLen);
    _ipdHoldLen += dataLen;
}

void SIM7600IPLib::_processProxyFrameIfPossible() {

    // Check for a complete frame
    if (_ipdHoldLen < 2) {
        return;
    }
    uint16_t frameLen = _ipdHold[0] << 8 | _ipdHold[1];
    if (_ipdHoldLen < frameLen) {
        return;
    }

    // Process the full frame
    _processProxyFrame(_ipdHold, frameLen);

    // If there's anything left, shift down to the start of the hold area
    if (_ipdHoldLen > frameLen) {
        for (uint32_t i = 0; i < (_ipdHoldLen - frameLen); i++)
            _ipdHold[i] = _ipdHold[frameLen + i];
    }
    _ipdHoldLen -= frameLen;
}

void SIM7600IPLib::_processProxyFrame(const uint8_t* frame, uint32_t frameLen) {

    if (frameLen >= 4) {

        //_log->debugDump("Proxy Frame:", frame, frameLen);

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
        else if (frame[2] == 0 &&
                 frame[3] == ClientFrameType::RESP_BIND_UDP) {
            if (frameLen != sizeof(ResponseBindUDP)) {
                _log->error("Invalid response");
                return;
            }
            ResponseBindUDP resp;
            memcpy(&resp, frame, frameLen);
            if (resp.rc != 0) {
                return;
            }

            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->bind(Channel(resp.id));
        }
        else if (frame[2] == ClientFrameType::RESP_SEND_TCP) {
            if (frameLen != 5) {
                _log->error("Invalid response");
                return;
            }
            //uint16_t id = frame[3] << 8 | frame[4];
        }
        else if (frame[2] == ClientFrameType::RESP_RECV_TCP) {
            if (frameLen < 5) {
                _log->error("Invalid response");
                return;
            }

            uint16_t id = frame[3] << 8 | frame[4];

            // Distribute it to the listeners
            for (uint32_t i = 0; i < _eventsLen; i++)
                // TODO: WRONG ADDRESS!!
                _events[i]->recv(Channel(id), frame + 5, frameLen - 5,
                    _lastAddr, _lastPort);
        }
        else if (frame[2] == 0 &&
                 frame[3] == ClientFrameType::RECV_DATA) {

            if (frameLen < 12) {
                _log->error("Invalid message ignored");
                return;
            }

            RecvData packet;
            memcpyLimited((uint8_t*)&packet, frame, frameLen, sizeof(packet));
            IPAddress addr(packet.addr);

            if (frameLen != packet.len) {
                _log->error("Invalid message ignored (2)");
                return;
            }

            //_log->info("Got UDP Data %u", (upacket.id);

            // Distribute it to the listeners
            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->recv(Channel(packet.id), packet.data, packet.len - 12,
                    addr, packet.port);
        }
        else if (frame[2] == ClientFrameType::RESP_CLOSE) {
            if (frameLen < 6) {
                _log->error("Invalid response");
                return;
            }

            uint16_t id = frame[3] << 8 | frame[4];

            // Distribute it to the listeners
            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->disc(Channel(id));
        }
        else {
            _log->info("Unsupported response type");
        }
    }    
}

void SIM7600IPLib::_queueSend(const uint8_t* d, uint32_t dl) {
    _sendQueue[_sendQueuePtr.getAndIncWritePtr()] = QueuedSend(d, dl);
    // Diagnostic only
    if (_sendQueuePtr.getOverflowCount() > 0) {
        _log->info("Send queue overflow %u", 
            _sendQueuePtr.getOverflowCount());
    }
    /*
    _sendQueue[_sendQueueWrPtr] = Send(d, dl);
    _sendQueueWrPtr++;
    // Deal with wrap
    if (_sendQueueWrPtr == _sendQueueSize) {
        _sendQueueWrPtr = 0;
    }
    */
}

void SIM7600IPLib::addEventSink(IPLibEvents* e) {
    if (_eventsLen < _maxEvents) {
        _events[_eventsLen++] = e;
    } else {
        panic_unsupported();
    }
}

void SIM7600IPLib::reset() {
    _state = State::INIT_0a;
}

bool SIM7600IPLib::isLinkUp() const {
    return _state == State::RUN || _state == State::SEND_1 ||
        _state == State::SEND_2 || _state == State::SEND_3;
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

    if (traceLevel > 1)
        _log->info("Sending %d", c.getId());
    
    RequestSendTCP req;
    req.len = len + 6;
    req.type = ClientFrameType::REQ_SEND_TCP;
    req.clientId = c.getId();
    memcpyLimited(req.contentPlaceholder, b, len, 2048);
    // Queue for delivery
    _queueSend((const uint8_t*)&req, req.len);
}

Channel SIM7600IPLib::createUDPChannel() {
    return Channel(_channelCount++, true);
}

void SIM7600IPLib::bindUDPChannel(Channel c, uint32_t localPort) {

    if (traceLevel > 1)
        _log->info("Binding channel %d to port %d", c.getId(), localPort);

    RequestBindUDP req;
    req.len = sizeof(req);
    req.type = ClientFrameType::REQ_BIND_UDP;
    req.id = c.getId();
    req.bindPort = (uint16_t)localPort;
    _queueSend((const uint8_t*)&req, sizeof(req));
}

void SIM7600IPLib::sendUDPChannel(const Channel& c, 
    const IPAddress& remoteIpAddr, uint32_t remotePort,
    const uint8_t* b, uint16_t len) {

    if (traceLevel > 1)
        _log->info("Sending %d", c.getId());
    
    RequestSendUDP req;
    req.len = len + 12;
    req.type = ClientFrameType::REQ_SEND_UDP;
    req.id = c.getId();
    req.addr = remoteIpAddr.getAddr();
    req.port = remotePort;
    memcpyLimited(req.data, b, len, 2048);
    // Queue for delivery
    _queueSend((const uint8_t*)&req, req.len);
}

}
