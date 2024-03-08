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

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/Log.h"

#include "common.h"
#include "Conference.h"
#include "machines/DNSMachine.h"

namespace kc1fsz {

static const uint32_t KEEP_ALIVE_INTERVAL_MS = 10 * 1000;
static const uint32_t TALKER_INTERVAL_MS = 1000;
static const uint32_t PING_INTERVAL_US = 15'000'000;

uint32_t Conference::_ssrcGenerator = 0xa000;
int Conference::traceLevel = 0;

Conference::Conference(Authority* auth, ConferenceOutput* out, Log* log,
    DNSMachine* addressingDNS) 
:   _authority(auth), 
    _output(out), 
    _log(log),
    _addressingDNS(addressingDNS) { 
    _pingTimer.setIntervalUs(PING_INTERVAL_US);
}

uint32_t Conference::getActiveStationCount() const {

    uint32_t result = 0;
    for (const Station& s : _stations) {
        if (s.active && s.authorized) {
            if (s.locked) {
                if (s.secondsSinceLastAudioRx() < 120) 
                    result++;
            } else {
                result++;
            }
        }
    }
    return result;
}

void Conference::dumpStations(Log* log) const {
    for (const Station& s : _stations) {
        if (s.active) {
            char addr[16];
            s.id.getAddr().formatAsDottedDecimal(addr, 16);
            log->info("Call: %s, ip: %s, auth: %d, lck: %d, tlk: %d/%d/%d",
                s.id.getCall().c_str(), addr, s.authorized, s.locked, 
                s.talker, s.secondsSinceLastAudioRx(), s.secondsSinceLastAudioTx());
        }
    }
}

void Conference::addRadio(CallSign cs, IPAddress addr) {

    _log->info("Adding radio %s", cs.c_str());

    for (Station& s : _stations) {
        if (!s.active) {
            s.reset();
            s.active = true;
            s.locked = true;
            s.id = StationID(addr, cs);
            s.authorized = true;
            s.lastRxStamp = time_ms();
            s.lastAudioRxStamp = time_ms();
            s.connectStamp = time_ms();
            break;
        }
    }
}

void Conference::authorize(StationID id) {

    _log->info("Station %s has been authorized", id.getCall().c_str());

    for (Station& s : _stations) {
        if (s.active && s.id == id) {
            s.authorized = true;
            return;
        }
    }

    _log->info("Manually adding conference participant");

    for (Station& s : _stations) {
        if (!s.active) {
            s.reset();
            s.active = true;
            s.id = id;
            s.authorized = true;
            s.lastRxStamp = time_ms();
            s.lastAudioRxStamp = time_ms();
            s.connectStamp = time_ms();
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
                s.lastAudioRxStamp = time_ms();
            }
        }
        else {
            s.talker = false;
            if (s.authorized) {                
                s.lastTxStamp = time_ms();
                s.lastAudioTxStamp = time_ms();
                _output->sendAudio(s.id.getAddr(), 
                    ssrc, s.seq++, frame, frameLen, fmt);
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

    prettyHexDump(data, dataLen, std::cout);

    // Look for PING back from Addressing Server and ignore
    if (isRTCPPINGPacket(data, dataLen)) {
        _lastPingRxStamp = time_ms();
        return;
    }


    // Look for OPEN packet from the Addressing Server and use
    // it to open the firewall for the incoming request. 
    //
    // NOTE: This doesn't do anything to validate or even remember
    // the calling station!  We're just trying to help with 
    // connectivity.
    if (isRTCPOPENPacket(data, dataLen)) {

        auto [ cs, ad ] = parseRTCPOPENPacket(data, dataLen);
        char addr[32];
        ad.formatAsDottedDecimal(addr, 32);
        _log->info("OPEN request received for %s %s", cs.c_str(), addr);
        
        // Send the OVER
        uint32_t ssrc = 0;
        const uint32_t packetSize = 128;
        uint8_t packet[packetSize];
        uint32_t packetLen = formatRTCPPacket_OVER(ssrc, packet, packetSize);      
        _output->sendText(ad, packet, packetLen);

        // Send the McAd
        packetLen = formatRTPPacket_McAD(packet, packetSize);      
        _output->sendAudio(ad, 0, 0, packet, packetLen, AudioFormat::TEXT);
        
        return;
    }

    if (!isRTCPPacket(data, dataLen)) {
        return;
    }    

    // Pull out the call sign from the RTCP message
    auto sourceStationId = _extractStationID(source, data, dataLen);
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
                s.reset();
                s.active = true;
                s.id = sourceStationId;
                s.connectStamp = time_ms();
                // We do this to avoid an immediate timeout
                s.lastRxStamp = time_ms();
                s.lastAudioRxStamp = time_ms();
                s.ssrc = _ssrcGenerator++;

                char buf[32];
                sourceStationId.getAddr().formatAsDottedDecimal(buf, 32);
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
        if (s.active && !s.locked) {
            _log->info("Dropping station %s", s.id.getCall().c_str());
            _sendBye(s.id);
            s.active = false;
        }
    }
}

bool Conference::run() {

    // Ping the Addressing Server to keep the link up
    // Ping the diagnostic server
    if (_pingTimer.poll()) {
        _pingTimer.reset();
        _sendPing();
    }

    // Station maintenance
    for (Station& s : _stations) {
        if (s.active) {
            // Check for the need to send outbound ping
            if (s.authorized && 
                time_ms() - s.lastTxStamp > KEEP_ALIVE_INTERVAL_MS) {
                _sendStationPing(s.id);
                s.lastTxStamp = time_ms();
            }
            // Look for timeouts (technical)
            if (!s.locked && 
                time_ms() - s.lastRxStamp > (KEEP_ALIVE_INTERVAL_MS * 3)) {
                _log->info("Timing out disconnected station %s", s.id.getCall().c_str());
                _sendBye(s.id);
                s.active = false;
            }
            // Look for timeouts (just not talking anymore)
            if (!s.locked &&
                s.secondsSinceLastAudioRx() > _silentTimeoutS) {
                _log->info("Timing out silent station %s", s.id.getCall().c_str());
                _sendBye(s.id);
                s.active = false;
            }
            // Look for timeouts (just not talking anymore)
            if (!s.locked &&
                s.secondsSinceLastAudioRx() > _idleTimeoutS &&
                s.secondsSinceLastAudioTx() > _idleTimeoutS) {
                _log->info("Timing out idle station %s", s.id.getCall().c_str());
                _sendBye(s.id);
                s.active = false;
            }
            // Time out talking
            if (s.talker && 
                time_ms() - s.lastAudioRxStamp > TALKER_INTERVAL_MS) {
                s.talker = false;
                _log->info("Station %s is finished talking", s.id.getCall().c_str());
            }
        }
    }
    return true;
}

void Conference::_sendPing() {

    if (traceLevel > 0)
        _log->info("Ping");

    if (_addressingDNS->isValid()) {
        // Make the SDES message and send
        {
            uint32_t ssrc = 0;
            const uint16_t packetSize = 64;
            uint8_t packet[packetSize];
            uint32_t packetLen = formatRTCPPacket_PING(ssrc, _callSign, 
                packet, packetSize); 
            _output->sendText(_addressingDNS->getAddress(), packet, packetLen);
        }
    }
}

void Conference::_sendStationPing(StationID id) {

    if (traceLevel > 0)
        _log->info("Ping to %s", id.getCall().c_str());

    // Make the SDES message and send
    {
        uint32_t ssrc = 0;
        const uint16_t packetSize = 128;
        uint8_t packet[packetSize];
        uint32_t packetLen = formatRTCPPacket_SDES(ssrc, 
            _callSign, _fullName, ssrc, packet, packetSize); 
        _output->sendText(id.getAddr(), packet, packetLen);
    }

    {
        const uint16_t packetSize = 128;
        uint8_t packet[packetSize];

        // Make the initial oNDATA message for the RTP port
        const uint16_t bufferSize = 128;
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
        
        _output->sendAudio(id.getAddr(), 0, 0, packet, packetLen, AudioFormat::TEXT);
    }
}

void Conference::_sendBye(StationID id) {
    const uint16_t packetSize = 128;
    uint8_t packet[packetSize];
    uint32_t packetLen = formatRTCPPacket_BYE(0, packet, packetSize);
    _output->sendText(id.getAddr(), packet, packetLen);
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
