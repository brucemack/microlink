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
#include <iostream>
#include <cstring>
#include <algorithm>

#include "../FixedString.h"

#include "../events/DNSLookupEvent.h"
#include "../events/TCPConnectEvent.h"
#include "../events/TCPDisconnectEvent.h"
#include "../events/TCPReceiveEvent.h"

#include "LookupMachine.h"

using namespace std;

namespace kc1fsz {

void LookupMachine::start(Context* ctx) {
    _foundTarget = false;
    _targetAddr = 0;
    // Launch the DNS resolution process
    ctx->startDNSLookup(_serverHostName);
    // We give the lookup 5 seconds to complete
    _setTimeoutMs(ctx->getTimeMs() + 5000);
    _state = DNS_WAIT;
}

void LookupMachine::processEvent(const Event* ev, Context* ctx) {

    // In this state we are doing nothing waiting to be started
    if (_state == IDLE) {
    }
    // In this state we are waiting for the DNS resolution to complete
    else if (_state == DNS_WAIT) {
        // Look for good completion
        if (ev->getType() == DNSLookupEvent::TYPE) {
            const DNSLookupEvent* evt = (DNSLookupEvent*)ev;
            // Start the process of opening the TCP connection
            _channel = ctx->createTCPChannel();
            ctx->connectTCPChannel(_channel, evt->addr);
            // We give the connect 1 second to complete
            _setTimeoutMs(ctx->getTimeMs() + 1000);
            _state = CONNECTING;
        }
        else if (_isTimedOut(ctx)) {
            _state = FAILED;
        }
    }
    else if (_state == CONNECTING) {
        if (ev->getType() == TCPConnectEvent::TYPE) {
            const TCPConnectEvent* evt = (TCPConnectEvent*)ev;
            // Grab the channel that is connected
            _channel = evt->getChannel();
            // Send the directory request message
            uint8_t buf[1];
            buf[0] = 's';
            ctx->sendTCPChannel(_channel, buf, 1);
            // We give the directory 15 seconds to complete
            _setTimeoutMs(ctx->getTimeMs() + 15000);
            _saveAreaPtr = 0;
            _headerSeen = false;
            _state = WAITING_FOR_DISCONNECT;            
        } 
        else if (_isTimedOut(ctx)) {
            _state = FAILED;
        }
    }
    else if (_state == WAITING_FOR_DISCONNECT) {

        // If we get data then accept it
        if (ev->getType() == TCPReceiveEvent::TYPE) {

            const TCPReceiveEvent* evt = (TCPReceiveEvent*)ev;

            if (!_foundTarget) {

                // Our goal is to find a complete record.  Assemble 
                // what we have already plus the new stuff for temporary 
                // processing.  
                const uint32_t workAreaSize = 64 + 256;
                uint8_t workArea[workAreaSize];
                memcpyLimited(workArea, _saveArea, _saveAreaPtr, workAreaSize);
                memcpyLimited(workArea + _saveAreaPtr, evt->getData(), evt->getDataLen(),
                    workAreaSize - _saveAreaPtr);
                uint32_t workAreaLen = _saveAreaPtr + evt->getDataLen();
                
                // Keep working on this work area until we can't make any more 
                // progress with the data in the workarea.
                while (workAreaLen > 0) {

                    // Hunt for the delimiters in the work area
                    uint16_t delimCount = 0;
                    uint16_t delimPoints[5];
                    bool fullRecordSeen = false;
                    uint32_t workAreaUsed = 0;

                    for (uint32_t i = 0; i < workAreaLen && !fullRecordSeen; i++) {                            
                        if (workArea[i] == 0x0a) {
                            delimPoints[delimCount++] = i;
                        }
                        // Check to see if we've seen a complete set of delimiters that we can process
                        if ((!_headerSeen && delimCount == 2) ||
                            (_headerSeen && delimCount == 5)) {
                            fullRecordSeen = true;
                        }
                        workAreaUsed++;
                    }

                    if (fullRecordSeen) {
                        if (_headerSeen) {
                            // Make sure the callsign and the IP address are under 31
                            // character to avoid any overflows
                            if (delimPoints[0] <= 31 && 
                                (delimPoints[4] - delimPoints[3]) <= 31) {
                                // Grab the callsign ad IP and see if it's what
                                // we're looking for.
                                char possibleCallSign[32];
                                char possibleIpAddr[32];

                                memcpyLimited((uint8_t*)possibleCallSign, workArea, 
                                    delimPoints[0], 31);
                                possibleCallSign[delimPoints[0]] = 0;

                                memcpyLimited((uint8_t*)possibleIpAddr, 
                                    workArea + delimPoints[3] + 1,
                                    delimPoints[4] - delimPoints[3], 31);
                                possibleIpAddr[delimPoints[4] - delimPoints[3]] = 0;

                                if (_targetCallSign == possibleCallSign) {
                                    _foundTarget = true;
                                    cout << possibleIpAddr << endl;
                                    _targetAddr = parseIP4Address(possibleIpAddr);
                                }
                            }
                        }
                        else {
                            _headerSeen = true;
                        }

                        // Shift down the consumed bytes so that we can consider them
                        // in the next iteration.
                        if (workAreaUsed == workAreaLen) {
                            workAreaLen = 0;
                        } else {
                            memcpy(workArea, workArea + workAreaUsed, workAreaLen - workAreaUsed);
                            workAreaLen -= workAreaUsed;
                        }
                    }

                    if (!fullRecordSeen) {
                        break;
                    }
                }

                // Anything that is left in the work area at this point gets
                // shifted into the _saveArea for consideration after a 
                // future data receipt (or disconnect).
                if (workAreaLen > 0) {
                    memcpyLimited(_saveArea, workArea, workAreaLen, 64);
                }
                _saveAreaPtr = std::min(workAreaLen, (uint32_t)64);
            }
        }
        // If we get a disconnect then move forward
        else if (ev->getType() == TCPDisconnectEvent::TYPE) {
            const TCPConnectEvent* evt = (TCPConnectEvent*)ev;
            if (evt->getChannel() == _channel) {
                // Parse the response to make sure we got what we expected
                if (true) {
                    _state = SUCCEEDED;
                } else {
                    // TODO: MESSAGE
                    _state = FAILED;
                }
            } else {
                // TODO: MESSAGE
                _state = FAILED;
            }
        }
        else if (_isTimedOut(ctx)) {
            // TODO: NEED MESSAGE
            _state = FAILED;
        }
    }
}

bool LookupMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool LookupMachine::isGood() const {
    return 
    _state == SUCCEEDED;
}

}
