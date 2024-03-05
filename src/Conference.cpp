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

void Conference::authorize(StationID id) {
    for (Station& s : _stations) {
        if (s.active && s.id == id) {
            s.authorized = true;
            break;
        }
    }
}

void Conference::deAuthorize(StationID id) {
    for (Station& s : _stations) {
        if (s.active && s.id == id) {
            s.active = false;
            s.authorized = false;
            break;
        }
    }
}

void Conference::processAudio(IPAddress sourceIp, 
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

    // If someone else is already talking then ignore any new audio
    StationID talker = _getTalker();
    if (!talker.isNull() && !(talker == source)) {
        return;
    }

    // This is the heart of the repeater
    for (Station& s : _stations) {
        if (s.id == source) {
            if (s.authorized) {
                s.talker = true;
                s.lastRxStamp = time_ms();
            }
        }
        else {
            s.talker = false;
            if (s.authorized) {                
                s.lastTxStamp = time_ms();
                _output->sendAudio(s.id, frame, frameLen, fmt);
            }
        }
    }
}

void Conference::processText(IPAddress source, 
    const uint8_t* frame, uint32_t frameLen) {

    // Pull out the call sign
    CallSign cs;
    StationID sourceStationId;

    // Check to see if this is a new station.
    bool active = false;
    for (const Station& s : _stations) {
        if (s.active && s.id == sourceStationId) {
            active = true;
            break;
        }
    }

    // If this is a new station then register it and launch an 
    // authorization.
    if (!active) {
        for (Station& s : _stations) {
            if (!s.active) {
                s.active = true;
                s.authorized = false;
                s.id = sourceStationId;
                _authority->validate(sourceStationId);
                char buf[32];
                formatIP4Address(sourceStationId.getAddr().getAddr(), buf, 32);
                _log->info("New connection request %s %s", buf, 
                    sourceStationId.getCall().c_str());
            }
        }    
    }

    for (Station& s : _stations) {
        if (s.active) {
            if (s.id == sourceStationId) {
                if (s.authorized) {
                    s.lastRxStamp = time_ms();
                }
            }
        }
    }
}

bool Conference::run() {
    for (Station& s : _stations) {
        if (s.active && s.authorized) {
            // Check for the need to send outbound ping
            if (time_ms() - s.lastTxStamp > KEEP_ALIVE_INTERVAL_MS) {
                _output->sendPing(s.id);
                s.lastTxStamp = time_ms();
            }
            // Look for timeouts
            if (time_ms() - s.lastRxStamp > (KEEP_ALIVE_INTERVAL_MS * 6)) {
                _output->sendBye(s.id);
                s.active = false;
            }
            // Time out talking
            if (s.talker && 
                time_ms() - s.lastRxStamp > TALKER_INTERVAL_MS) {
                s.talker = false;
            }
        }
    }
    return true;
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
