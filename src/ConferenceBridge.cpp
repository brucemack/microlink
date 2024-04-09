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
    _radio0Addr(0xff000002),
    _radio0GSMQueuePtr(_radio0GSMQueueSize) {   
}

void ConferenceBridge::run() {
    // Keep delivering audio to the radio
    _serviceRadio0GSMQueue();
}

void ConferenceBridge::reset() {    

    _log->info("ConferenceBridge reset");

    // Get UDP connections created
    _rtcpChannel = _ctx->createUDPChannel();
    _rtpChannel = _ctx->createUDPChannel();

    // Start the RTCP socket setup (RTCP)
    _ctx->bindUDPChannel(_rtcpChannel, RTCP_PORT);
    // Start the RTP socket setup
    _ctx->bindUDPChannel(_rtpChannel, RTP_PORT);
}

void ConferenceBridge::bind(Channel ch) {
    if (ch == _rtcpChannel) {
        _log->info("RTCP bind successful");
    }
    else if (ch == _rtpChannel) {
        _log->info("RTP bind successful");
    }
}

void ConferenceBridge::recv(Channel ch, const uint8_t* data, uint32_t dataLen, 
    IPAddress fromAddr, uint16_t fromPort) {

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
            // This is the performance-critical path
            _conf->processAudio(fromAddr, remoteSSRC, remoteSeq,
                d, 33 * 4, AudioFormat::GSMFR4X);
        }
        else {
            _log->info("Unrecognized packet");
        }
    }
}

bool ConferenceBridge::play(const int16_t* pcmAudio, uint32_t frameLen)  {

    uint8_t gsmAudio[4 * 33];
    uint8_t* gsmAudioPtr = gsmAudio;
    const int16_t* pcmAudioPtr = pcmAudio;

    // It's safe to re-use this across frames:
    Parameters params;
    
    for (int f = 0; f < 4; f++) {
        _gsmEncoder0.encode(pcmAudioPtr, &params);
        params.pack(gsmAudioPtr);
        pcmAudioPtr += 160;
        gsmAudioPtr += 33;
    }

    _conf->processAudio(_radio0Addr, _radio0Ssrc, _radio0Seq++,
        gsmAudio, 33 * 4, AudioFormat::GSMFR4X);

    return true;
}

void ConferenceBridge::sendAudio(const IPAddress& dest, uint32_t ssrc, uint16_t seq,
    const uint8_t* gsmData, uint32_t gsmDataLen, AudioFormat fmt) {
    if (fmt == AudioFormat::TEXT) {
        if (dest == _radio0Addr) {
            // Radio can't handle the text,ignore
        }
        else {
            _ctx->sendUDPChannel(_rtpChannel, dest, RTP_PORT, gsmData, gsmDataLen);
        }
    } else if (fmt == AudioFormat::GSMFR4X && gsmDataLen == (4 * 33)) {
        if (dest == _radio0Addr) {
            const uint8_t* gsmDataPtr = gsmData;
            for (uint32_t f = 0; f < 4; f++) {
                // Queue the audio data headed to the radio to allow the frames 
                // going to other non-radio nodes to bypass quickly. Note that 
                // there is no decoding/playing going on here - just queuing.
                _writeRadio0GSMQueue(gsmDataPtr, 33);
                gsmDataPtr += 33;
            }
        }
        else {
            // Pass the GSMx4 data along to the other nodes
            uint8_t packet[144];
            uint32_t packetLen = formatRTPPacket(seq, ssrc, gsmData, packet, 144);
            _ctx->sendUDPChannel(_rtpChannel, dest, RTP_PORT, packet, packetLen);
        }
    } else {
        panic_unsupported();
    }
}

void ConferenceBridge::sendText(const IPAddress& dest,
    const uint8_t* data, uint32_t dataLen) {

    // The radio can't handle text
    if (dest == _radio0Addr) {
        return;
    }

    if (traceLevel > 0) {
        char addr[32];
        dest.formatAsDottedDecimal(addr, 32);
        _log->info("Sending to %s", addr);
        prettyHexDump(data, dataLen, cout);
    }

    _ctx->sendUDPChannel(_rtcpChannel, dest, RTCP_PORT, data, dataLen);
}

void ConferenceBridge::_writeRadio0GSMQueue(const uint8_t* gsmFrame, uint32_t gsmFrameLen) {

    if (gsmFrameLen != 33) {
        panic_unsupported();
    }

    // Move the buffer into a circular queue
    memcpy(_radio0GSMQueue[_radio0GSMQueuePtr.getAndIncWritePtr()], 
        gsmFrame, gsmFrameLen);
}

void ConferenceBridge::_serviceRadio0GSMQueue() {

    if (_radio0GSMQueuePtr.isEmpty()) {
        return;
    }

    // We handle one GSM frame on each shot to avoid tying up the event
    // look for longer than necessary.  Once 4 frames have been accumulated
    // and decoded we pass a 160x4 PCM frame along for audio output.

    // Convert the GSM data to PCM16 audio so that it can be transmitted.
    const uint8_t* gsmAudioPtr = _radio0GSMQueue[_radio0GSMQueuePtr.getAndIncReadPtr()];
    Parameters params;
    params.unpack(gsmAudioPtr);
    _gsmDecoder0.decode(&params, _pcmFrame + _pcmFramePtr);

    _pcmFramePtr += 160;
    // Check to see if we've gotten 4 frames
    if (_pcmFramePtr == 4 * 160) {
        _radio0->play(_pcmFrame, 160 * 4);
        _pcmFramePtr = 0;
    }
}

}
