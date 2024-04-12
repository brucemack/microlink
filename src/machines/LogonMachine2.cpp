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
#include <sys/time.h>
#include <iostream>
#include <cstring>

#include "kc1fsz-tools/Log.h"

#include "../UserInfo.h"
#include "../Conference.h"

#include "DNSMachine.h"
#include "LogonMachine2.h"

using namespace std;

namespace kc1fsz {

// Time between successful logons
static const uint32_t LOGON_INTERVAL_MS = 5 * 60 * 1000;
// Time after a failed logon.  Used to avoid hammering the Addressing 
// server 
static const uint32_t PAUSE_INTERVAL_MS = 30 * 1000;
static const uint32_t DNS_TIMEOUT_MS = 10000;
static const uint32_t CONNECT_TIMEOUT_MS = 5000;
static const uint32_t LOGON_TIMEOUT_MS = 10 * 1000;
// How long to wait for the DNS address to be avalble before re-trying 
// the sequence. 
static const uint32_t DNS_WAIT_MS = 10'000;

int LogonMachine2::traceLevel = 0;

LogonMachine2::LogonMachine2(IPLib* ctx, Log* log,
    DNSMachine* dm, const FixedString& versionId)
:   _ctx(ctx),
    _log(log),
    _dnsMachine(dm),
    _versionId(versionId) {
    _channel = Channel(0, false);
    _logonRespPtr = 0;
    _setState(State::IDLE);
}

uint32_t LogonMachine2::secondsSinceLastLogon() const {
    return ms_since(_lastLogonStamp) / 1000;
}

void LogonMachine2::conn(Channel ch) {

    if (_isState(State::CONNECT_WAIT) && ch == _channel) {

        // Append the conference size to the location string
        char augmentedLocation[28];
        strcpyLimited(augmentedLocation, _location.c_str(), 28);
        char count[16];
        snprintf(count, 16, "[%lu]", _conf->getActiveStationCount());
        strcatLimited(augmentedLocation, count, 28);        

        // Build the logon message
        uint8_t buf[256];
        uint32_t bufLen = createOnlineMessage(buf, 256, _callSign, _password, 
            FixedString(augmentedLocation), _versionId, _emailAddr);
        _ctx->sendTCPChannel(_channel, buf, bufLen);
        // Get ready to accumulate the response
        _logonRespPtr = 0;
        // We give the logon 10 seconds to complete
        _setState(State::DISCONNECT_WAIT, LOGON_TIMEOUT_MS, State::FAILED);
    }
}

void LogonMachine2::recv(Channel ch, 
    const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
    uint16_t fromPort) {

    if (_isState(State::DISCONNECT_WAIT) && ch == _channel) {
        // Accumulate the data (or as much as possible)
        uint32_t spaceLeft = _logonRespSize - _logonRespPtr;
        uint32_t l = std::min(spaceLeft, dataLen);
        memcpyLimited(_logonResp + _logonRespPtr, data, l, spaceLeft);
        _logonRespPtr += l;
    }
}

void LogonMachine2::disc(Channel ch) {

    if (_isState(State::DISCONNECT_WAIT) && ch == _channel) {
        // Parse the response to make sure we got what we expected
        if (_logonRespPtr >= 1 && _logonResp[0] == 'O' && _logonResp[1] == 'K') {
            _lastLogonStamp = time_ms();
            if (traceLevel > 0)
                _log->info("Logon succeeded");
            _setState(State::SUCCEEDED);
        } else {
            _log->error("Logon failed");
            _setState(State::FAILED);
        }
    }
}

void LogonMachine2::_process(int state, bool entry) {

    if (traceLevel > 0) {
        if (entry)
            _log->info("LogonMachine2: state=%d", _getState());
    }

    if (_isState(State::IDLE)) {
        if (_dnsMachine->isValid()) {

            // Start the process of opening the TCP connection to the 
            // Addressing server
            _channel = _ctx->createTCPChannel();
            if (!_channel.isGood()) {
                _setState(State::FAILED);
                return;
            } 
            _ctx->connectTCPChannel(_channel, _dnsMachine->getAddress(), 
                _serverPort);
            _setState(State::CONNECT_WAIT, CONNECT_TIMEOUT_MS, State::FAILED);
        }
        else {
            _log->info("Waiting on Addressing Server address");
            // We give some time for the link to come up before
            // going back to the idle state
            _setState(State::DNS_WAIT, DNS_WAIT_MS, State::IDLE);
        }
    }
    else if (_isState(State::SUCCEEDED)) {
        if (_channel.isGood()) {
            _ctx->closeChannel(_channel);
            _channel = Channel(0, false);
        }
        _setState(State::WAIT, LOGON_INTERVAL_MS, State::IDLE);
    }
    else if (_isState(State::FAILED)) {
        if (_channel.isGood()) {
            _ctx->closeChannel(_channel);
            _channel = Channel(0, false);
        }
        _setState(State::WAIT, PAUSE_INTERVAL_MS, State::IDLE);
    }
}

}
