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

#include "kc1fsz-tools/Log.h"

#include "../UserInfo.h"

#include "LookupMachine3.h"

using namespace std;

namespace kc1fsz {

static const uint32_t CONNECT_TIMEOUT_MS = 2000;
static const uint32_t DNS_TIMEOUT_MS = 5000;
static const uint32_t VERIFY_TIMEOUT_MS = 5000;

int LookupMachine3::traceLevel = 0;

LookupMachine3::LookupMachine3(IPLib* ctx, Log* log)
:   _ctx(ctx),
    _log(log),
    _serverPort(0) { 
    _setState(State::IDLE);
}

void LookupMachine3::validate(StationID id) {

    _targetCallSign = id.getCall();
    _targetAddr = id.getAddr();

    char buf[16];
    _targetAddr.formatAsDottedDecimal(buf, 16);
    _log->info("Request to validate %s from %s", _targetCallSign.c_str(), buf);

    // This counter is used to manage retries
    _stateCount = 3;
    _setState(State::REQUEST);
}

void LookupMachine3::dns(HostName name, IPAddress addr) {

    if (_isState(State::DNS_WAIT) && name == _serverHostName) {

        // Start the process of opening the TCP connection to the 
        // Addressing server
        _channel = _ctx->createTCPChannel();
        if (!_channel.isGood()) {
            _setState(State::FAILED);
            return;
        } 
        _ctx->connectTCPChannel(_channel, addr, _serverPort);
        _setState(State::CONNECT_WAIT, CONNECT_TIMEOUT_MS, State::FAILED);
    }
}

void LookupMachine3::conn(Channel ch) {

    if (_isState(State::CONNECT_WAIT) && ch == _channel) {
        // Get ready to collect result
        _saveAreaUsed = 0;
        // Send the directory verify message
        char buf[64];
        // NOTE: This one is a capital "V"
        sprintf(buf, "V%s\n", _targetCallSign.c_str());
        _ctx->sendTCPChannel(_channel, (const uint8_t*)buf, strlen(buf));
        _setState(State::DISCONNECT_WAIT, VERIFY_TIMEOUT_MS, State::FAILED);
    }
}

void LookupMachine3::recv(Channel ch, 
    const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
    uint16_t fromPort) {

    if (_isState(State::DISCONNECT_WAIT) && ch == _channel) {
        memcpyLimited(_saveArea + _saveAreaUsed, data, dataLen,
            _saveAreaSize - _saveAreaUsed);
        // TODO WARNING: CLIENT CAN OVERFLOW US!
        _saveAreaUsed +=  dataLen;
    }
}

void LookupMachine3::disc(Channel ch) {

    if (_isState(State::DISCONNECT_WAIT) && ch == _channel) {

        // Hunt for the delimiters in the _saveArea
        uint16_t delimCount = 0;
        uint16_t delimPoints[4];

        for (uint32_t i = 0; i < _saveAreaUsed; i++) {                            
            if (_saveArea[i] == 0x0a) {
                delimPoints[delimCount++] = i;
            }
        }

        if (delimCount >= 4) {
            // Make sure the IP address is under 31 character to avoid 
            // any overflows.
            if ((delimPoints[3] - delimPoints[2]) <= 31) {

                char ipAddr[32];
                memcpyLimited((uint8_t*)ipAddr, 
                    _saveArea + delimPoints[2] + 1,
                    delimPoints[3] - delimPoints[2], 31);
                ipAddr[delimPoints[3] - delimPoints[2]] = 0;
                IPAddress authAddr = parseIP4Address(ipAddr);

                if (authAddr == _targetAddr) {
                    _log->info("Authorized");
                    _conf->authorize(StationID(_targetAddr, _targetCallSign));
                    _setState(State::SUCCEEDED);
                } 
                // This is a special case.  If the validation request has a zero
                // address then the address is not considered in the validation.
                // This is what allows us to manually join stations to the conference.
                else if (_targetAddr == IPAddress(0)) {
                    _log->info("Authorized (manual)");
                    _conf->authorize(StationID(authAddr, _targetCallSign));
                    _setState(State::SUCCEEDED);
                } 
                else {
                    _log->info("Unauthorized, address mismatch");
                    _conf->deAuthorize(StationID(_targetAddr, _targetCallSign));
                    // Even though the station was not authorized, the state
                    // machine ran to completion, so this is a success.
                    _setState(State::SUCCEEDED);
                }
            } 
            // We get into this case when the response is malformed
            else {
                _log->error("Invalid response from addressing server");
                _conf->deAuthorize(StationID(_targetAddr, _targetCallSign));
                // Even though the station was not authorized, the state
                // machine ran to completion, so this is a success.
                _setState(State::SUCCEEDED);
            }
        }
    }
}

void LookupMachine3::_process(int state, bool entry) {

    if (traceLevel > 0) {
        if (entry)
            _log->info("LookupMachine3: state=%d", _getState());
    }

    if (_isState(State::REQUEST)) {
        // Launch the DNS resolution process
        _ctx->queryDNS(_serverHostName);
        _setState(State::DNS_WAIT, DNS_TIMEOUT_MS, State::FAILED);
    }
    else if (_isState(State::SUCCEEDED)) {
        if (_channel.isGood()) {
            _ctx->closeChannel(_channel);
            _channel = Channel(0, false);
        }
        _setState(State::IDLE);
    }
    else if (_isState(State::FAILED)) {
        if (_channel.isGood()) {
            _ctx->closeChannel(_channel);
            _channel = Channel(0, false);
        }
        if (_stateCount > 0) {
            _stateCount--;
            _setState(State::REQUEST);
            _log->info("LookupMachine3 retrying");
        }
        else {
            _setState(State::IDLE);
            _log->error("LookupMachine3 failed");
        }
    }
}

}
