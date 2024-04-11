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
#include <time.h>
#include <iostream>

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/Log.h"

#include "SNTPClient.h"

#define SNTP_PORT (123)
#define SNTP_POLLING_INTERVAL_MS (90 * 1000)
#define SNTP_RETRY_INTERVAL_MS (10 * 1000)

using namespace std;

namespace kc1fsz {

static const char* NTP_SERVER_NAME = "pool.ntp.org";
//static const char* NTP_SERVER_NAME = "time.google.com";

SNTPClient::SNTPClient(Log* log, IPLib* ctx)
:   _log(log),
    _ctx(ctx) {    
    _setState(State::SLEEPING, SNTP_RETRY_INTERVAL_MS, State::AWAKE);
}

void SNTPClient::reset() {    
    // Get UDP connections created
    _channel = _ctx->createUDPChannel();
    _ctx->bindUDPChannel(_channel, SNTP_PORT);
}

void SNTPClient::dns(HostName name, IPAddress addr) {
    if (name == NTP_SERVER_NAME) {
        // Send a message to the address we got
        //char buf[32];
        //addr.formatAsDottedDecimal(buf, 32);
        //_log->info("Got NTP address %s", buf);

        // Current time
        uint32_t ut = get_epoch_time();

        // Build an SNTP request
        uint8_t sntpReq[68];
        memset(sntpReq, 0, 68);
        sntpReq[0] = 0b00'100'011;
        sntpReq[1] = 0;
        sntpReq[2] = 6;
        sntpReq[3] = 0xec;
        sntpReq[12] = 'U';
        sntpReq[13] = 'N';
        sntpReq[14] = 'K';
        sntpReq[15] = 'O';
        // We put the current client time into the TX time field.
        // This is important to avoid the duplicate supression
        // feature of the server.
        sntpReq[40] = (ut >> 24) & 0xff;
        sntpReq[41] = (ut >> 16) & 0xff;
        sntpReq[42] = (ut >>  8) & 0xff;
        sntpReq[43] = (ut      ) & 0xff;

        _ctx->sendUDPChannel(_channel, addr, SNTP_PORT, sntpReq, 68);
        _setState(State::RESPONSE_WAIT, SNTP_RETRY_INTERVAL_MS, State::FAILED);
    }
}

void SNTPClient::bind(Channel ch) {
}

void SNTPClient::recv(Channel ch, const uint8_t* data, uint32_t dataLen, 
    IPAddress fromAddr, uint16_t fromPort) {
    if (ch == _channel) {
        //_log->infoDump("SNTP", data, dataLen);
        if (isValidSNTPResponse(data, dataLen)) {
            uint32_t now = getTimeFromSNTPResponse(data, dataLen) -
                2208988800UL;
            // Set the time
            set_epoch_time(now);
        }
        _setState(State::SUCCEEDED);
    }
}

void SNTPClient::_process(int state, bool entry) {
    if (_isState(State::AWAKE)) {
        // Start a DNS request
        _ctx->queryDNS(NTP_SERVER_NAME);
        _setState(State::DNS_WAIT, SNTP_RETRY_INTERVAL_MS, State::FAILED);
    }
    else if (_isState(State::SUCCEEDED)) {
        _setState(State::SLEEPING, SNTP_POLLING_INTERVAL_MS, State::AWAKE);
    }
    else if (_isState(State::FAILED)) {
        _setState(State::SLEEPING, SNTP_RETRY_INTERVAL_MS, State::AWAKE);
    }
}

}


