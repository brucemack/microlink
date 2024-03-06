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
#include "pico/platform.h"

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/AudioOutputContext.h"

#include "common.h"
#include "UserInfo.h"
#include "Conference.h"
#include "ConferenceBridge.h"

using namespace std;

namespace kc1fsz {

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

static const uint32_t CHANNEL_SETUP_TIMEOUT_MS = 1000;
static const uint32_t SEND_TIMEOUT_MS = 1000;

int ConferenceBridge::traceLevel = 0;

ConferenceBridge::ConferenceBridge(IPLib* ctx, UserInfo* userInfo, Log* log, 
    AudioOutputContext* radio0)
:   _ctx(ctx),
    _userInfo(userInfo),
    _conf(0),
    _log(log),
    _radio0(radio0),
    _radio0Addr(0xf000) {   

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
            }
            if (traceLevel > 1) {
                prettyHexDump(data, dataLen, cout);
            }

            _conf->processText(fromAddr, data, dataLen);
        } 
        else if (ch == _rtpChannel) {

            if (traceLevel > 0) {
                _log->info("ConferenceBridge: GOT RTP DATA");
            }
            if (traceLevel > 1) {
                prettyHexDump(data, dataLen, cout);
            }

            if (isOnDataPacket(data, dataLen)) {
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
                _conf->processAudio(fromAddr, remoteSSRC, remoteSeq,
                    d, 33 * 4, AudioFormat::GSMFR4X);
            }
            else {
                _log->info("Unrecognized packet");
            }
        }
    }
}

void ConferenceBridge::sendAudio(StationID dest, uint32_t ssrc, uint16_t seq,
    const uint8_t* data, uint32_t dataLen, AudioFormat fmt) {
    if (fmt == AudioFormat::TEXT) {
        if (dest.getAddr() == _radio0Addr) {
            // Radio can't handle the text
        }
        else {
            _ctx->sendUDPChannel(_rtpChannel, dest.getAddr(), RTP_PORT, 
                data, dataLen);
        }
    } else if (fmt == AudioFormat::GSMFR4X && dataLen == (4 * 33)) {
        if (dest.getAddr() == _radio0Addr) {
            // Convert the GSM data to PCM16 audio so that it can be 
            // transmitted.
            int16_t pcmAudio[160 * 4];
            int16_t* pcmAudioPtr = pcmAudio;
            const uint8_t* gsmAudioPtr = data;
            Parameters params;

            for (uint32_t f = 0; f < 4; f++) {
                // TODO: PROVIDE AN UNPACK THAT CONTAINS STATE
                kc1fsz::PackingState state;

                params.unpack(gsmAudioPtr, &state);
                _gsmDecoder0.decode(&params, pcmAudioPtr);
                pcmAudioPtr += 160;
                gsmAudioPtr += 33;
            }
            _radio0->play(pcmAudio, 4 * 160);
        }
        else {
            // TODO: CLEAN UP
            uint8_t gsmFrames[4][33];
            const uint8_t* s = data;
            for (uint32_t f = 0; f < 4; f++) {
                for (uint32_t p = 0; p < 33; p++) {
                    gsmFrames[f][p] = *s;
                    s++;
                }        
            }

            uint8_t packet[144];
            uint32_t packetLen = formatRTPPacket(seq, ssrc, gsmFrames, packet, 144);
            _ctx->sendUDPChannel(_rtpChannel, dest.getAddr(), RTP_PORT, packet, packetLen);
        }
    } else {
        panic_unsupported();
    }
}

void ConferenceBridge::sendText(StationID dest,
    const uint8_t* data, uint32_t dataLen) {
    
    if (traceLevel > 0) {
        _log->info("Sending to %s", dest.getCall().c_str());
        prettyHexDump(data, dataLen, cout);
    }

    _ctx->sendUDPChannel(_rtcpChannel, dest.getAddr(), RTCP_PORT, 
        data, dataLen);
}

void ConferenceBridge::_process(int state, bool entry) {
    if (traceLevel > 0) {
        if (entry)
            _log->info("ConferenceBridge state %d", _getState());
    }
}

}
