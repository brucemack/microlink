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

using namespace std;

namespace kc1fsz {

// Time after a failed DNS.  Used to avoid hammering the server.
static const uint32_t PAUSE_INTERVAL_MS = 10'000;
// How long we wait for the DNS server
static const uint32_t DNS_TIMEOUT_MS = 10'000;
// How long to wait for the Internet link to come up before re-trying 
// the sequence. 
static const uint32_t LINK_WAIT_MS = 10'000;

int DNSMachine::traceLevel = 0;

DNSMachine::DNSMachine(IPLib* ctx, UserInfo* userInfo, Log* log, uint32_t intervalMs) 
:   _ctx(ctx),
    _userInfo(userInfo),
    _log(log),
    _intervalMs(intervalMs),
    _isValid(false) {
    _setState(State::IDLE);
}

void DNSMachine::dns(HostName name, IPAddress addr) {
    if (_isState(State::DNS_WAIT) && name == _hostName) {
        _address = addr;
        _isValid = true;
        _setState(State::SUCCEEDED);
    }
}

void DNSMachine::_process(int state, bool entry) {

    if (traceLevel > 0) {
        if (entry)
            _log->info("DNSMachine: state=%d", _getState());
    }

    if (_isState(State::IDLE)) {
        if (_ctx->isLinkUp()) {
            // Launch the DNS resolution process
            _ctx->queryDNS(_hostName);
            _setState(State::DNS_WAIT, DNS_TIMEOUT_MS, State::FAILED);
        }
        else {
            if (traceLevel > 0)
                _log->info("Link not ready, DNS waiting");
            // We give some time for the link to come up before
            // going back to the idle state
            _setState(State::LINK_WAIT, LINK_WAIT_MS, State::IDLE);
        }
    }
    else if (_isState(State::SUCCEEDED)) {
        _setState(State::WAIT, _intervalMs, State::IDLE);
    }
    else if (_isState(State::FAILED)) {
        _setState(State::WAIT, PAUSE_INTERVAL_MS, State::IDLE);
    }
}

}
