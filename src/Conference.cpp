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

using namespace std;

namespace kc1fsz {

static const uint32_t KEEP_ALIVE_INTERVAL_MS = 10 * 1000;
// This controls the amount of silence used to decide that 
// someone has stopped talking.
static const uint32_t TALKER_INTERVAL_MS = 500;
static const uint32_t PING_INTERVAL_US = 15'000'000;
static const uint32_t MONITOR_INTERVAL_US = 30'000'000;

uint32_t Conference::_ssrcGenerator = 0xa000;
int Conference::traceLevel = 0;

Conference::Conference(Authority* auth, ConferenceOutput* out, Log* log,
    DNSMachine* addressingDNS, DNSMachine* monitorDNS) 
:   _authority(auth), 
    _output(out), 
    _log(log),
    _addressingDNS(addressingDNS),
    _monitorDNS(monitorDNS) { 
    _pingTimer.setIntervalUs(PING_INTERVAL_US);
    _monitorTimer.setIntervalUs(MONITOR_INTERVAL_US);
    _startStamp = time_ms();
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
            log->info("  seqerr: %lu, maxgap: %lu", 
                s.rxSeqErr, s.longestRxGapMs);
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

    const uint32_t now = time_ms();

    // Figure out whether this packet is coming from an authorized 
    // source and whether someone else is talking already.
    bool isSourceAutorized = false;
    bool isNonSourceTalking = false;

    for (const Station& s : _stations) {
        if (s.active && s.authorized) {
            if (s.id.getAddr() == sourceIp) {
                isSourceAutorized = true;
            } 
            else {
                if (s.talker) {
                    isNonSourceTalking = true;
                }
            }
        }
    }

    if (!isSourceAutorized || isNonSourceTalking) {
        return;
    }

    if (fmt == AudioFormat::TEXT) {
        // When the oNDATA is followed by a \r this is general station 
        // information.
        if (frame[6] == 0x0d) {
        } 
        // When the onDATA is followed by "CONF" this is general station
        // information.
        else if (frameLen >= 10 && 
            frame[6] == 'C' && frame[7] == 'O' && frame[8] == 'N' && frame[9] == 'F') {

        } 
        // Otherwise, this is chat
        else {
            _log->debugDump("Chat", frame, frameLen);

            // Extract the call and message (oNDATAxxxxxx>mmmmm\r)
            int state = 0;
            char call[32] = { 0 };
            uint32_t callLen = 0;
            char msg[64] = { 0 };
            uint32_t msgLen = 0;
            
            for (uint32_t p = 6; p < frameLen; p++) {
                if (state == 0) {
                    if (frame[p] == '>') {
                        state = 1;
                    } else {
                        if (callLen < 31) {
                            call[callLen] = frame[p];
                            call[callLen + 1] = 0;
                            callLen++;
                        }
                    }
                }
                else if (state == 1) {
                    if (frame[p] == 0x0d) {
                        state = 2;
                    } else {
                        if (msgLen < 63) {
                            msg[msgLen] = frame[p];
                            msg[msgLen + 1] = 0;
                            msgLen++;
                        }
                    }
                }
            }

            _processChat(sourceIp, call, msg);
        }
        return;
    }

    // This is the heart of the repeater
    for (Station& s : _stations) {
        if (!s.active || !s.authorized) {
            continue;
        }
        if (s.id.getAddr() == sourceIp) {
            // Look for the non-talking to talking transition
            if (!s.talker) {
                _log->info("Station %s is talking", s.id.getCall().c_str());
                s.talker = true;
                s.audioRxPacketCount = 0;
            } 
            // This is continuation of talking - do some QOS checking
            else {
                if (seq != 0 && seq != s.lastRxSeq + 1) {
                    s.rxSeqErr++;
                }
                uint32_t gap = (now - s.lastAudioRxStamp);
                if (gap > s.longestRxGapMs) {
                    //_log->info("Longest gap %lu", gap);
                    s.longestRxGapMs = gap;
                }
            }
            s.audioRxPacketCount++;
            s.lastRxStamp = now;
            s.lastAudioRxStamp = now;
            s.lastRxSeq = seq;
        }
        else {
            s.talker = false;
            s.lastAudioTxStamp = now;
            _lastActivityStamp = now;
            _output->sendAudio(s.id.getAddr(), ssrc, s.seq++, 
                frame, frameLen, fmt);
        }
    }
}

void Conference::processText(IPAddress source, 
    const uint8_t* data, uint32_t dataLen) {

    // TEMP!    
    char by[16];
    source.formatAsDottedDecimal(by, 16);
    //cout << by << endl;
    //prettyHexDump(data, dataLen, cout);

    if (source == _monitorAddr) {
        _processMonitorText(source, data, dataLen);
        return;
    }

    // Look for PING back from Addressing Server and ignore
    if (isRTCPPINGPacket(data, dataLen)) {
        _lastPingRxStamp = time_ms();
        return;
    }

    // If this is a bye, shut down the station
    if (isRTCPByePacket(data, dataLen)) {
        for (Station& s : _stations) {
            if (s.active) {
                if (s.id.getAddr() == source) {
                    _log->info("BYE from %s, disconnected", s.id.getCall().c_str());
                    s.active = false;
                    break;
                }
            }
        }
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
    auto call = _extractCallSign(data, dataLen);
    if (call.isNull()) {
        return;
    }

    // See if there is an existing Station for this call
    Station* activeStation = 0;
    for (Station& s : _stations) {
        if (s.active && s.id.getCall() == call) {
            activeStation = &s;
            break;
        }
    }

    // Existing station checks
    if (activeStation) {
        // If the station isn't authorized yet then exit
         if (!activeStation->authorized)
            return;
        // If the station is authorized using a different IP?
        if (!(activeStation->id.getAddr() == source)) {
            _log->info("Station %s has moved IPs", call.c_str());
            return;
        }
        // Record the receive activity
        activeStation->lastRxStamp = time_ms();
    }
    // If this is a new station then register it and launch an 
    // authorization to find out if it is allowed to join
    // the conference.
    else {
        // Look for an open slot (if any)
        bool good = false;
        for (Station& s : _stations) {
            if (!s.active) {
                s.reset();
                s.active = true;
                s.id = StationID(source, call);
                s.connectStamp = time_ms();
                // We do this to avoid an immediate timeout
                s.lastRxStamp = time_ms();
                s.lastAudioRxStamp = time_ms();
                s.ssrc = _ssrcGenerator++;

                char buf[32];
                source.formatAsDottedDecimal(buf, 32);
                _log->info("New connection request %s from %s", 
                    s.id.getCall().c_str(), buf);

                // This is asynchronous
                _authority->validate(s.id);

                good = true;
                break;
            }
        }    
        if (!good) {
            // Jut ignore
            char buf[32];
            source.formatAsDottedDecimal(buf, 32);
            _log->info("New connection request %s from %s ignored, full.", 
                call.c_str(), buf);
        }
    }
}

void Conference::_processMonitorText(IPAddress source, 
    const uint8_t* data, uint32_t dataLen) {
    //prettyHexDump(data, dataLen, std::cout);
    _lastMonitorRxStamp = time_ms();
}

void Conference::_processChat(const IPAddress& source,
    const char* call, const char* msg) {

    // Look for "call xxxxxx"
    if (strlen(msg) > 5 && msg[0] == 'c' && msg[1] == 'a' && 
        msg[2] == 'l' && msg[3] == 'l' && msg[4] == ' ') {
        // Isolate the target callsign without spaces, all upper case
        char targetCall[32];
        uint32_t targetCallLen = 0;
        for (uint32_t msgPtr = 5; 
            msg[msgPtr] != 0 && targetCallLen < 31; 
            msgPtr++) {
            if (msg[msgPtr] != ' ') {
                targetCall[targetCallLen++] = std::toupper(msg[msgPtr]);
            }
        }
        targetCall[targetCallLen++] = 0;

        _log->info("Calling %s ...", targetCall);
        // TODO: NEED TONE CONFIRMATION!
        
        // This is asynchronous
        _authority->validate(StationID(IPAddress(0), CallSign(targetCall)));
    }
    // Look for "drop xxxxxx"
    else if (strlen(msg) > 5 && msg[0] == 'd' && msg[1] == 'r' && 
        msg[2] == 'o' && msg[3] == 'p' && msg[4] == ' ') {
        // Isolate the target callsign without spaces, all upper case
        char targetCall[32];
        uint32_t targetCallLen = 0;
        for (uint32_t msgPtr = 5; 
            msg[msgPtr] != 0 && targetCallLen < 31; 
            msgPtr++) {
            if (msg[msgPtr] != ' ') {
                targetCall[targetCallLen++] = std::toupper(msg[msgPtr]);
            }
        }
        targetCall[targetCallLen++] = 0;

        _drop(CallSign(targetCall));
        // TODO: NEED TONE CONFIRMATION!
    }
}

CallSign Conference::_extractCallSign(const uint8_t* data, uint32_t dataLen) {
    if (isRTCPSDESPacket(data, dataLen)) {
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
                return CallSign(callSignStr);
            }
        }
    }
    return CallSign();
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

void Conference::_drop(const CallSign& cs) {
    for (Station& s : _stations) {
        if (s.active && !s.locked && s.id.getCall() == cs) {
            _log->info("Dropping station %s", s.id.getCall().c_str());
            _sendBye(s.id);
            s.active = false;
        }
    }
}

bool Conference::run() {

    // Ping the Addressing Server to keep the link up
    //if (_pingTimer.poll()) {
    //    _pingTimer.reset();
    //    _sendPing();
    //}

    /*
    // Ping the monitor server 
    if (_monitorTimer.poll()) {
        _monitorTimer.reset();
        _sendMonitorPing();
    }
    */

    // Station maintenance
    for (Station& s : _stations) {
        if (s.active) {
            // Check for the need to send outbound ping text. This is very 
            // important to keep the connection up and running.
            if (s.authorized && 
                time_ms() - s.lastTextTxStamp > KEEP_ALIVE_INTERVAL_MS) {
                _sendStationPing(s.id);
                s.lastTextTxStamp = time_ms();
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
            // Look for timeouts (just not talking or listening anymore)
            if (!s.locked &&
                s.secondsSinceLastAudioRx() > _idleTimeoutS &&
                s.secondsSinceLastAudioTx() > _idleTimeoutS) {
                _log->info("Timing out idle station %s", s.id.getCall().c_str());
                _sendBye(s.id);
                s.active = false;
            }
            // Time out talking
            if (s.talker && 
                s.msSinceLastAudioRx() > TALKER_INTERVAL_MS) {
                s.talker = false;
                _log->info("Station %s finished talking %lu %lu", 
                    s.id.getCall().c_str(), s.rxSeqErr, s.longestRxGapMs);
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

void Conference::_sendMonitorPing() {

    if (traceLevel > 0)
        _log->info("Monitor Ping");

    if (_monitorTxCount % 10 == 0) {
        if (_monitorDNS->isValid()) {
            _monitorAddr = _monitorDNS->getAddress();
        }
        else {
            return;
        }
    }

    // Make a diagnostic packet
    const uint32_t packetSize = 128;
    char packet[packetSize];
    snprintf(packet, packetSize, "MicroLink,%s,%s,%lu\n", 
        VERSION_ID, 
        _callSign.c_str(),
        (time_ms() - _startStamp) / 1000);
    _output->sendText(_monitorAddr, (const uint8_t*)packet, strlen(packet));

    _monitorTxCount++;
}

void Conference::_sendStationPing(const StationID& id) {

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
        const uint16_t packetSize = 256;
        uint8_t packet[packetSize];

        // Make the initial oNDATA message for the RTP port
        const uint16_t bufferSize = 256 - 16;
        char buffer[bufferSize];
        buffer[0] = 0;
        strcatLimited(buffer, "oNDATA\r", bufferSize);
        strcatLimited(buffer, _callSign.c_str(), bufferSize);
        strcatLimited(buffer, "\r", bufferSize);
        strcatLimited(buffer, _fullName.c_str(), bufferSize);
        strcatLimited(buffer, "\r", bufferSize);
        strcatLimited(buffer, _location.c_str(), bufferSize);
        strcatLimited(buffer, "\r", bufferSize);
        strcatLimited(buffer, "MicroLink V ", bufferSize);
        strcatLimited(buffer, VERSION_ID, bufferSize);
        strcatLimited(buffer, "\r", bufferSize);
        strcatLimited(buffer, "\rIn conference:\r", bufferSize);

        char msg[64];
        for (Station& s : _stations) {
            if (s.active && !s.locked) {
                snprintf(msg, 64, "%s\r", s.id.getCall().c_str());
                strcatLimited(buffer, msg, bufferSize);
            }
        } 
        snprintf(msg, 64, "\rDiag st=%lu, act=%lu, mon=%lu\r", 
            getSecondsSinceStart(),
            getSecondsSinceLastActivity(),
            getSecondsSinceLastMonitorRx());
        strcatLimited(buffer, msg, bufferSize);

        snprintf(msg, 64, "WIFI RSSI=%d, RX=%lu\r", 
            _wifiRssi,
            _rxPower);
        strcatLimited(buffer, msg, bufferSize);

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

}
