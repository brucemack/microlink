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
#include "kc1fsz-tools/CommContext.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/SendEvent.h"
#include "kc1fsz-tools/AudioOutputContext.h"

#include "gsm-0610-codec/Parameters.h"

#include "../common.h"
#include "../UserInfo.h"

#include "QSOConnectMachine.h"
#include "QSOFlowMachine.h"

using namespace std;

namespace kc1fsz {

// Significance?
// d8 20 a2 e1 5a 50 00 49 24 92 49 24 50 00 49 24 92 49 24 50 00 49 24 92 49 24 50 00 49 24 92 49 24
static const uint8_t SPEC_FRAME[33] = {
 0xd8, 0x20, 0xa2, 0xe1, 0x5a, 0x50, 0x00, 0x49, 
 0x24, 0x92, 0x49, 0x24, 0x50, 0x00, 0x49, 0x24, 
 0x92, 0x49, 0x24, 0x50, 0x00, 0x49, 0x24, 0x92, 
 0x49, 0x24, 0x50, 0x00, 0x49, 0x24, 0x92, 0x49, 
 0x24 };

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

// How often we send activity on the UDP connections to keep the 
// connection alive.
static const uint32_t KEEP_ALIVE_INTERVAL_MS = 10000;
// How long we wait in silence before unkeying on TX
static const uint32_t TX_TAIL_INTERVAL_MS = 1000;
// How long we want for the SEND OK message 
static const uint32_t SEND_OK_TIMEOUT_MS = 500;

int QSOFlowMachine::traceLevel = 0;

QSOFlowMachine::QSOFlowMachine(CommContext* ctx, UserInfo* userInfo, 
    AudioOutputContext* audioOutput, bool useLocalSsrc) 
:   _ctx(ctx),
    _userInfo(userInfo),
    _audioOutput(audioOutput),
    _useLocalSsrc(useLocalSsrc) {
}

void QSOFlowMachine::start() {
    _lastRTCPKeepAliveSentMs = 0;
    _lastRTPKeepAliveSentMs = 0;
    _lastRecvMs = time_ms();
    _txAudioWriteCount = 0;
    _txAudioSentCount = 0;
    _lastTxAudioTime = 0;
    _lastRxAudioTime = 0;
    _lastRxAudioSeq = 0;
    _audioSeqErrors = 0;
    _stopRequested = false;
    _byeReceived = false;
    _state = State::OPEN_RX;
    _gsmEncoder.reset();
    _gsmDecoder.reset();
}

void QSOFlowMachine::cleanup() {
    _peerAddr = IPAddress();
}

void QSOFlowMachine::_processRXReceive(const UDPReceiveEvent* evt) {

    if (evt->getChannel() == _rtcpChannel) {

        if (traceLevel > 0) {
            cout << "QSOConnectMachine: (RX) GOT RTCP DATA" << endl;
            prettyHexDump(evt->getData(), evt->getDataLen(), cout);
        }

        // Check for BYE
        if (isRTCPByePacket(evt->getData(), evt->getDataLen())) {
            _byeReceived = true;
        }
    }
    else if (evt->getChannel() == _rtpChannel) {

        if (isRTPAudioPacket(evt->getData(), evt->getDataLen())) {

            if (traceLevel > 0) {
                cout << "QSOFLowMachine: (RX) Audio" << endl;
            }
            
            _lastRxAudioTime = time_ms();

            // Unload the GSM frames from the RTP packet
            uint16_t remoteSeq = 0;
            //uint32_t remoteSSRC = 0;
            const uint8_t* d = evt->getData();

            remoteSeq = ((uint16_t)d[2] << 8) | (uint16_t)d[3];
            //remoteSSRC = ((uint16_t)d[8] << 24) | ((uint16_t)d[9] << 16) | 
            //    ((uint16_t)d[10] << 8) | ((uint16_t)d[11]);

            // Check for out-of-sequence
            if (remoteSeq <= _lastRxAudioSeq) {
                _audioSeqErrors++;
            }

            const uint32_t framesPerPacket = 4;
            const uint32_t samplesPerFrame = 160;
            int16_t pcmData[framesPerPacket * samplesPerFrame];
            uint32_t pcmPtr = 0;
            Parameters params;

            // Process each of the GSM frame independently/sequentially.
            // Each packet received will result in a call to 
            // _adioOutput->play() with 160x4 samples.
            const uint8_t* frame = (d + 12);
            for (uint16_t f = 0; f < framesPerPacket; f++) {
                // TODO: EXPLAIN?
                //if (memcmp(SPEC_FRAME, frame, 33) == 0) {
                if (false) {
                    for (uint32_t i = 0; i < 160; i++) {
                        pcmData[pcmPtr + i] = 0;
                    }
                } 
                else {
                    // TODO: PROVIDE AN UNPACK THAT CONTAINS STATE
                    kc1fsz::PackingState state;
                    params.unpack(frame, &state);
                    _gsmDecoder.decode(&params, pcmData + pcmPtr);
                }
                pcmPtr += samplesPerFrame;
                frame += 33;
            }

            // Hand off 160x4 samples to the audio context to play the audio
            _audioOutput->play(pcmData, framesPerPacket * samplesPerFrame);
        } 
        else if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
            if (traceLevel > 0) {
                cout << "QSOFLowMachine: (RX) oNDATA" << endl;
                prettyHexDump(evt->getData(), evt->getDataLen(), cout);
            }
            _processONDATA(evt->getData(), evt->getDataLen());
        }
    }

    _lastRecvMs = time_ms();
}

void QSOFlowMachine::_processTXReceive(const UDPReceiveEvent* evt) {

    if (evt->getChannel() == _rtcpChannel) {

        if (traceLevel > 0) {
            cout << "QSOConnectMachine: GOT RTCP DATA" << endl;
            prettyHexDump(evt->getData(), evt->getDataLen(), cout);
        }    

        // Check for BYE
        if (isRTCPByePacket(evt->getData(), evt->getDataLen())) {
            _byeReceived = true;
        }
    }
    else if (evt->getChannel() == _rtpChannel) {

        if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
            if (traceLevel > 0) {
                cout << "QSOFLowMachine: (TX) oNDATA" << endl;
                prettyHexDump(evt->getData(), evt->getDataLen(), cout);
            }
            _processONDATA(evt->getData(), evt->getDataLen());
        }
    }

    _lastRecvMs = time_ms();
}

void QSOFlowMachine::_processONDATA(const uint8_t* d, uint32_t len) {
    // Make sure the message is null-terminated one way or the other
    char temp[64];
    memcpyLimited((uint8_t*)temp, d, len, 63);
    temp[std::min((uint32_t)63, len)] = 0;
    // Here we skip past the oNDATA part when we report the message
    _userInfo->setOnData(temp + 6);
}

void QSOFlowMachine::_sendONDATA() {

    const uint16_t packetSize = 128;
    uint8_t packet[packetSize];

    // Make the initial oNDATA message for the RTP port
    const uint16_t bufferSize = 64;
    char buffer[bufferSize];
    buffer[0] = 0;
    strcatLimited(buffer, "oNDATA\r", bufferSize);
    strcatLimited(buffer, _callSign.c_str(), bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    strcatLimited(buffer, "MicroLink V ", bufferSize);
    strcatLimited(buffer, VERSION_ID, bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    strcatLimited(buffer, _fullName.c_str(), bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    strcatLimited(buffer, _location.c_str(), bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    uint32_t packetLen = QSOConnectMachine::formatOnDataPacket(buffer, _ssrc, packet, packetSize);
    _ctx->sendUDPChannel(_rtpChannel, _peerAddr, RTP_PORT, packet, packetLen);
}

void QSOFlowMachine::_sendBYE() {

    const uint16_t packetSize = 128;
    uint8_t packet[packetSize];
    uint32_t packetLen = formatRTCPPacket_BYE(0, packet, packetSize);
    _ctx->sendUDPChannel(_rtcpChannel, _peerAddr, RTCP_PORT, packet, packetLen);
}

void QSOFlowMachine::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        // Suppress tick - too noisy
        if (ev->getType() != TickEvent::TYPE) {
            cout << "QSOFlowMachine: state=" << _state 
                << " event=" << ev->getType() <<  endl;
        }
    }

    // Deal with event types that can arrive at any time
    if (ev->getType() == UDPReceiveEvent::TYPE) {

        auto evt = static_cast<const UDPReceiveEvent*>(ev);

        // Ingore other peers
        if (evt->getAddress().getAddr() != _peerAddr.getAddr()) {
            return;
        }

        if (_state == State::OPEN_RX ||
            _state == State::OPEN_RX_RTCP_PING_0 ||
            _state == State::OPEN_RX_RTCP_PING_1 ||
            _state == State::OPEN_RX_RTP_PING_0 ||
            _state == State::OPEN_RX_RTP_PING_1 ||
            _state == State::OPEN_RX_STOP_0) {
            _processRXReceive(evt);
        } else if (_state == State::OPEN_TX ||
            _state == State::OPEN_TX_AUDIO_1 ||
            _state == State::OPEN_TX_RTCP_PING_0 ||
            _state == State::OPEN_TX_RTCP_PING_1) {
            _processTXReceive(evt);
        }
    }

    // Now deal with state-specific activity
    if (_state == State::OPEN_RX) {
        // Look for a shutdown request
        if (_stopRequested || _byeReceived) {
            _sendBYE();
            _setState(State::OPEN_RX_STOP_0, SEND_OK_TIMEOUT_MS);
        }
        // Look to see if we should key up and change to TX mode
        else if (_txAudioWriteCount > _txAudioSentCount) {
            _gsmEncoder.reset();
            _state = State::OPEN_TX;
        }
        // Do a general check for failure on the other station
        else if (time_ms() > _lastRecvMs + (3 * KEEP_ALIVE_INTERVAL_MS)) {
            _userInfo->setStatus("Remote station not responding");
            _state = State::FAILED;
        }
        // Check to see if a RTCP keep-alive is needed
        else if (time_ms() > _lastRTCPKeepAliveSentMs + KEEP_ALIVE_INTERVAL_MS) {
            _state = State::OPEN_RX_RTCP_PING_0;
        }
        // Check to see if a RTP keep-alive is needed
        else if (time_ms() > _lastRTPKeepAliveSentMs + KEEP_ALIVE_INTERVAL_MS) {
            _state = State::OPEN_RX_RTP_PING_0;
        } 
    }
    else if (_state == State::OPEN_RX_RTCP_PING_0) {

        const uint16_t packetSize = 128;
        uint8_t packet[packetSize];

        // Make the SDES message and send
        uint32_t ssrc = _useLocalSsrc ? _ssrc : 0;
        uint32_t packetLen = QSOConnectMachine::formatRTCPPacket_SDES(ssrc, 
            _callSign, _fullName, _ssrc, packet, packetSize); 
        _ctx->sendUDPChannel(_rtcpChannel, _peerAddr, RTCP_PORT, packet, packetLen);

        _lastRTCPKeepAliveSentMs = time_ms();

        _setState(State::OPEN_RX_RTCP_PING_1, SEND_OK_TIMEOUT_MS);
    }
    else if (_state == State::OPEN_RX_RTP_PING_0) {

        _sendONDATA();
        _lastRTPKeepAliveSentMs = time_ms();

        _setState(State::OPEN_RX_RTP_PING_1, SEND_OK_TIMEOUT_MS);
    }
    else if (_state == State::OPEN_RX_RTCP_PING_1 ||
             _state == State::OPEN_RX_RTP_PING_1) {

        if (ev->getType() == SendEvent::TYPE) {
            const SendEvent* evt = static_cast<const SendEvent*>(ev);
            if (evt->isGood()) {
                _state = State::OPEN_RX;
            }
            // There's really nothing we can do if the send fails, so just 
            // display an error and keep going.
            else {
                _userInfo->setStatus("Send failed, ignoring");
                _state = State::OPEN_RX;
            }
        }
        else if (_isTimedOut()) {
            char buf[64];
            snprintf(buf, 64, "Timeout 2 %d", 0);
            _userInfo->setStatus(buf);
            _state = State::FAILED;
        }
    }
    else if (_state == State::OPEN_RX_STOP_0) {
        if (ev->getType() == SendEvent::TYPE) {
            _userInfo->setStatus("QSO ended");
            _state = State::SUCCEEDED;
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus("Timeout during QSO stop");
            _state = State::FAILED;
        }
    }
    else if (_state == State::OPEN_TX) {

        // Look for pending outbound audio
        if (_txAudioWriteCount > _txAudioSentCount) {

            if (traceLevel > 0) {
                cout << "QSOFlowMachine: TX audio packet " << _txAudioSentCount << endl;
            }

            uint8_t gsmFrames[4][33];
            uint32_t slot = _txAudioSentCount % _txAudioBufDepth;
            int16_t* frame = _txAudioBuf[slot];

            for (int f = 0; f < 4; f++) {
                Parameters params;
                _gsmEncoder.encode(frame, &params);
                frame += 160;
                PackingState state;
                params.pack(gsmFrames[f], &state);
            }

            uint8_t packet[144];
            uint32_t packetLen = formatRTPPacket(_txAudioSentCount, 
                0, gsmFrames, packet, 144);
            
            _ctx->sendUDPChannel(_rtpChannel, _peerAddr, RTP_PORT, packet, packetLen);

            _txAudioSentCount++;

            _setState(State::OPEN_TX_AUDIO_1, SEND_OK_TIMEOUT_MS);
        }
        // Check to see if the transmit activity has ended and we
        // should be switching back into RX mode
        else if (time_ms() > _lastTxAudioTime + TX_TAIL_INTERVAL_MS) {
            if (traceLevel > 0) {
                cout << "QSOFlowMachine: TX timed out, back to RX" << endl;
            }
            _gsmDecoder.reset();
            _state = State::OPEN_RX;
        }
        // Do a general check for failure on the other station
        else if (time_ms() > _lastRecvMs + (3 * KEEP_ALIVE_INTERVAL_MS)) {
            _userInfo->setStatus("Remote station not responding");
            _state = State::FAILED;
        }
        // Check to see if a RTCP keep-alive is needed
        else if (time_ms() > _lastRTCPKeepAliveSentMs + KEEP_ALIVE_INTERVAL_MS) {
            _state = State::OPEN_TX_RTCP_PING_0;
        }
    }
    else if (_state == State::OPEN_TX_RTCP_PING_0) {

        const uint16_t packetSize = 128;
        uint8_t packet[packetSize];

        // Make the SDES message and send
        uint32_t packetLen = QSOConnectMachine::formatRTCPPacket_SDES(0, _callSign, _fullName, _ssrc, packet, packetSize); 
        _ctx->sendUDPChannel(_rtcpChannel, _peerAddr, RTCP_PORT, packet, packetLen);

        _lastRTCPKeepAliveSentMs = time_ms();

        _setState(State::OPEN_TX_RTCP_PING_1, SEND_OK_TIMEOUT_MS);
    }
    else if (_state == State::OPEN_TX_AUDIO_1 ||
             _state == State::OPEN_TX_RTCP_PING_1) {

        if (ev->getType() == SendEvent::TYPE) {
            const SendEvent* evt = static_cast<const SendEvent*>(ev);
            if (evt->isGood()) {
                _state = State::OPEN_TX;
            }
            // There's really nothing we can do if the send fails, so just 
            // display an error and keep going.
            else {
                _userInfo->setStatus("Send failed, ignoring");
                _state = State::OPEN_TX;
            }
        }
        else if (_isTimedOut()) {
            char buf[64];
            snprintf(buf, 64, "Timeout 3 %d", 0);
            _userInfo->setStatus(buf);
            _state = State::FAILED;
        }
    }
}

bool QSOFlowMachine::requestCleanStop() {
    _stopRequested = true;
    return true;    
}

bool QSOFlowMachine::txAudio(const int16_t* frame) {

    // Is the buffer full?
    if ((_txAudioWriteCount - _txAudioSentCount) >= _txAudioBufDepth) {
        return false;        
    }

    // Capture the new frame in the buffer
    uint32_t slot = _txAudioWriteCount % _txAudioBufDepth;
    for (uint32_t i = 0; i < 160 * 4; i++) {
        _txAudioBuf[slot][i] = frame[i];
    }
    _txAudioWriteCount++;
    // Used to manage the "tail" of the transmission
    _lastTxAudioTime = time_ms();

    return true;
}

bool QSOFlowMachine::isDone() const {
    return (_state == SUCCEEDED || _state == FAILED);
}

bool QSOFlowMachine::isGood() const {
    return (_state == SUCCEEDED);
}

}
