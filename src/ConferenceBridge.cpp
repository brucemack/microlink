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
#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/Log.h"

#include "common.h"
#include "UserInfo.h"
#include "Conference.h"
#include "ConferenceBridge.h"

using namespace std;

namespace kc1fsz {

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

static const uint32_t CHANNEL_SETUP_TIMEOUT_MS = 250;
static const uint32_t SEND_TIMEOUT_MS = 1000;

static const char* FAILED_MSG = "Station connection failed";

int ConferenceBridge::traceLevel = 0;

ConferenceBridge::ConferenceBridge(IPLib* ctx, UserInfo* userInfo, Log* log)
:   _ctx(ctx),
    _userInfo(userInfo),
    _conf(0),
    _log(log) {   

    // Get UDP connections created
    _rtcpChannel = _ctx->createUDPChannel();
    _rtpChannel = _ctx->createUDPChannel();

    // Start the RTCP socket setup (RTCP)
    _ctx->bindUDPChannel(_rtcpChannel, RTCP_PORT);
    _setState(State::IN_SETUP_1, CHANNEL_SETUP_TIMEOUT_MS, State::FAILED);
}

void ConferenceBridge::bind(Channel ch) {
    if (_isState(State::IN_SETUP_1) && ch == _rtcpChannel) {
        // Start the RTP socket setup
        _ctx->bindUDPChannel(_rtpChannel, RTP_PORT);
        _setState(State::IN_SETUP_2, CHANNEL_SETUP_TIMEOUT_MS, State::FAILED);
    }
    else if (_isState(State::IN_SETUP_2) && ch == _rtpChannel) {
        // Start listening
        _userInfo->setStatus("Ready to receive");
        _setState(State::WAITING);
    }
}

void ConferenceBridge::recv(Channel ch, const uint8_t* data, uint32_t dataLen, 
    IPAddress fromAddr, uint16_t fromPort) {

    if (_isState(State::WAITING)) {

        if (ch == _rtcpChannel) {

            if (traceLevel > 0) {
                _log->info("ConferenceBridge: GOT RTCP DATA");
                prettyHexDump(data, dataLen, cout);
            }

            if (_conf)
                _conf->processText(fromAddr, data, dataLen);
        } 
        else if (ch == _rtpChannel) {

            if (traceLevel > 0) {
                _log->info("ConferenceBridge: GOT RTP DATA");
                //prettyHexDump(data, dataLen, cout);
            }

            if (isOnDataPacket(data, dataLen)) {
                if (_conf) 
                    _conf->processAudio(fromAddr, 0, 0, 
                        data, dataLen, AudioFormat::TEXT);
            }
            else if (dataLen == 144) {
                const uint8_t* d = data;
                uint16_t remoteSeq = 0;
                uint32_t remoteSSRC = 0;
                remoteSeq = ((uint16_t)d[2] << 8) | (uint16_t)d[3];
                remoteSSRC = ((uint16_t)d[8] << 24) | ((uint16_t)d[9] << 16) | 
                    ((uint16_t)d[10] << 8) | ((uint16_t)d[11]);
                d += 12;
                if (_conf)
                    _conf->processAudio(fromAddr, remoteSSRC, remoteSeq,
                        d, 33 * 4, AudioFormat::GSMFR4X);
            }
            else {
                _log->info("Unrecognized packet");
            }
        }
    }
}

void ConferenceBridge::_process(int state, bool entry) {
    if (traceLevel > 0) {
        if (entry)
            _log->info("ConferenceBridge state %d", _getState());
    }
}

}
