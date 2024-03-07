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

#include "LogonMachine2.h"
#include "MonitorMachine.h"

using namespace std;

namespace kc1fsz {

static const uint32_t DNS_TIMEOUT_MS = 5000;
static const uint32_t POLL_INTERVAL_MS = 30 * 1000;
static const int16_t DIAG_PORT = 5197;

int MonitorMachine::traceLevel = 0;

MonitorMachine::MonitorMachine(IPLib* ctx, UserInfo* userInfo, Log* log)
:   _ctx(ctx),
    _userInfo(userInfo),
    _log(log),
    _lm(0) { 
    _setState(State::INIT);
    _startStamp = time_ms();
}

void MonitorMachine::dns(HostName name, IPAddress addr) {
    if (_isState(State::DNS_WAIT) && name == _serverHostName) {
        _serverAddr = addr;
        _setState(State::SEND);
    }
}

void MonitorMachine::recv(Channel ch, 
    const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
    uint16_t fromPort) {
    if (ch == _channel) {
        prettyHexDump(data, dataLen, cout);
    }
}

void MonitorMachine::_process(int state, bool entry) {

    if (traceLevel > 0) {
        if (entry)
            _log->info("MonitorMachine: state=%d", _getState());
    }

    if (_isState(State::INIT)) {
        if (_ctx->isLinkUp()) {
            _channel = _ctx->createUDPChannel();
            _ctx->bindUDPChannel(_channel, DIAG_PORT);
            _setState(State::IDLE);
        }
        else {
            // If link isn't available then wait some time to avoid hammering
            _setState(State::LINK_WAIT, DNS_TIMEOUT_MS, State::INIT);
        }
    }
    else if (_isState(State::IDLE)) {
        if (_ctx->isLinkUp()) {
            // Launch the DNS resolution process
            _ctx->queryDNS(_serverHostName);
            _setState(State::DNS_WAIT, DNS_TIMEOUT_MS, State::FAILED);
        }
        else {
            // If link isn't available then wait some time to avoid hammering
            _setState(State::LINK_WAIT, DNS_TIMEOUT_MS, State::IDLE);
        }
    }
    else if (_isState(State::SEND)) {
        // Make a diagnostic packet
        const uint32_t packetSize = 128;
        char packet[packetSize];
        snprintf(packet, packetSize, "MicroLink,%s,%s,%lu,%lu\n", 
            VERSION_ID, 
            _callSign.c_str(),
            (time_ms() - _startStamp) / 1000,
            _lm->secondsSinceLastLogon());
        _ctx->sendUDPChannel(_channel, _serverAddr, DIAG_PORT, (const uint8_t*)packet, 
            strlen(packet));
        _setState(State::WAIT, POLL_INTERVAL_MS, State::IDLE);
    }
    else if (_isState(State::FAILED)) {
        _log->error("Failed");
        _setState(State::IDLE);
    }
}

}
