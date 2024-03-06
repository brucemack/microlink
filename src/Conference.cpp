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
#include "kc1fsz-tools/Log.h"

#include "common.h"
#include "Conference.h"

namespace kc1fsz {

static const uint32_t KEEP_ALIVE_INTERVAL_MS = 10 * 1000;
static const uint32_t TALKER_INTERVAL_MS = 1000;

uint32_t Conference::_ssrcGenerator = 0xa000;

void Conference::authorize(StationID id) {

    _log->info("Station %s has been authorized", id.getCall().c_str());

    for (Station& s : _stations) {
        if (s.active && s.id == id) {
            s.authorized = true;
            break;
        }
    }
}

void Conference::deAuthorize(StationID id) {

    _log->info("Station %s has not been authorized", id.getCall().c_str());

    for (Station& s : _stations) {
        if (s.active && s.id == id) {
            _sendBye(s.id);
            s.active = false;
            s.authorized = false;
            break;
        }
    }
}

void Conference::processAudio(IPAddress sourceIp, 
    uint32_t ssrc, uint16_t seq,
    const uint8_t* frame, uint32_t frameLen, AudioFormat fmt) {

    // Figure out what station this is
    StationID source;
    for (const Station& s : _stations) {
        if (s.active && s.authorized && s.id.getAddr() == sourceIp) {
            source = s.id;
            break;
        }
    }

    if (source.isNull()) {
        return;
    }

    // TODO: HANDLE TEXT
    if (fmt == AudioFormat::TEXT) {
        return;
    }

    // If someone else is already talking then ignore any new audio
    StationID talker = _getTalker();
    if (!talker.isNull() && !(talker == source)) {
        return;
    }

    // This is the heart of the repeater
    for (Station& s : _stations) {
        if (s.id == source) {
            if (s.authorized) {
                // Look for the talking transition
                if (!s.talker) {
                    _log->info("Station %s is talking", s.id.getCall().c_str());
                }
                s.talker = true;
                s.lastRxStamp = time_ms();
            }
        }
        else {
            s.talker = false;
            if (s.authorized) {                
                s.lastTxStamp = time_ms();
                _output->sendAudio(s.id, ssrc, frame, frameLen, fmt);
            }
        }
    }
}

void Conference::processText(IPAddress source, 
    const uint8_t* data, uint32_t dataLen) {

    // If this is a bye, shut down the station
    if (isRTCPByePacket(data, dataLen)) {
        for (Station& s : _stations) {
            if (s.active) {
                if (s.id.getAddr() == source) {
                    _log->info("Station %s disconnected", s.id.getCall().c_str());
                    s.active = false;
                    break;
                }
            }
        }
        return;
    }

    if (!isRTCPPacket(data, dataLen)) {
        return;
    }    

    // Pull out the call sign from the RTCP message
    StationID sourceStationId = _extractStationID(source, data, dataLen);
    if (sourceStationId.isNull()) {
        return;
    }

    // Check to see if this is a new station.
    bool active = false;
    for (const Station& s : _stations) {
        if (s.active && s.id == sourceStationId) {
            active = true;
            break;
        }
    }

    // If this is a new station then register it and launch an 
    // authorization to find out if it is allowed to join
    // the conference.
    if (!active) {
        for (Station& s : _stations) {
            if (!s.active) {
                s.active = true;
                s.authorized = false;
                s.id = sourceStationId;
                s.connectStamp = time_ms();
                // We do this to avoid an immediate timeout
                s.lastRxStamp = time_ms();
                s.ssrc = _ssrcGenerator++;

                char buf[32];
                formatIP4Address(sourceStationId.getAddr().getAddr(), buf, 32);
                _log->info("New connection request %s from %s", 
                    sourceStationId.getCall().c_str(), buf);

                // This is asynchronous
                _authority->validate(sourceStationId);

                break;
            }
        }    
    }

    // Record the receive activity
    for (Station& s : _stations) {
        if (s.active) {
            if (s.id == sourceStationId) {
                if (s.authorized) {
                    s.lastRxStamp = time_ms();
                }
                break;
            }
        }
    }
}

StationID Conference::_extractStationID(IPAddress source, const uint8_t* data,
    uint32_t dataLen) {

    // Pull out the callsign
    SDESItem items[8];
    uint32_t ssrc = 0;
    uint32_t itemCount = parseSDES(data, dataLen, &ssrc, items, 8);

    for (uint32_t item = 0; item < itemCount; item++) {
        if (items[item].type == 2) {
            char callSignAndName[64];
            items[item].toString(callSignAndName, 64);
            // Strip off the call (up to the first space)
            char callSignStr[32];
            uint32_t i = 0;
            // Leave space for null
            for (i = 0; i < 31 && callSignAndName[i] != ' '; i++)
                callSignStr[i] = callSignAndName[i];
            callSignStr[i] = 0;
            return StationID(source, CallSign(callSignStr));
        }
    }

    return StationID();
}

void Conference::dropAll() {
    for (Station& s : _stations) {
        if (s.active) {
            _log->info("Dropping station %s", s.id.getCall().c_str());
            _sendBye(s.id);
            s.active = false;
        }
    }
}

bool Conference::run() {
    for (Station& s : _stations) {
        if (s.active && s.authorized) {
            // Check for the need to send outbound ping
            if (time_ms() - s.lastTxStamp > KEEP_ALIVE_INTERVAL_MS) {
                _sendPing(s.id);
                s.lastTxStamp = time_ms();
            }
            // Look for timeouts
            if (time_ms() - s.lastRxStamp > (KEEP_ALIVE_INTERVAL_MS * 6)) {
                _log->info("Timing out station %s", s.id.getCall().c_str());
                _sendBye(s.id);
                s.active = false;
            }
            // Time out talking
            if (s.talker && 
                time_ms() - s.lastRxStamp > TALKER_INTERVAL_MS) {
                s.talker = false;
                _log->info("Station %s is finished talking", s.id.getCall().c_str());
            }
        }
    }
    return true;
}

void Conference::_sendPing(StationID id) {

    // Make the SDES message and send
    {
        uint32_t ssrc = 0;
        const uint16_t packetSize = 128;
        uint8_t packet[packetSize];
        uint32_t packetLen = formatRTCPPacket_SDES(ssrc, 
            _callSign, _fullName, ssrc, packet, packetSize); 
        _output->sendText(id, packet, packetLen);
    }

    {
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
        uint32_t packetLen = formatOnDataPacket(buffer, 0, packet, packetSize);
        
        _output->sendAudio(id, 0, packet, packetLen, AudioFormat::TEXT);
    }
}

void Conference::_sendBye(StationID id) {
    const uint16_t packetSize = 128;
    uint8_t packet[packetSize];
    uint32_t packetLen = formatRTCPPacket_BYE(0, packet, packetSize);
    _output->sendText(id, packet, packetLen);
}

StationID Conference::_getTalker() const {
    for (const Station& s : _stations) {
        if (s.active && s.authorized && s.talker) {
            return s.id;
        }
    }
    return StationID();
}

}
