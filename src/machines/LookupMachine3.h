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
#ifndef _LookupMachine3_h
#define _LookupMachine3_h

#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/IPLib.h"

#include "../StateMachine2.h"
#include "../Conference.h"

namespace kc1fsz {

class UserInfo;

/**
 * A state machine used for managing the EL directory lookup.
*/
class LookupMachine3 : public StateMachine2, public IPLibEvents, public Authority {
public:

    static int traceLevel;

    LookupMachine3(IPLib* ctx, UserInfo* userInfo, Log* log);

    void setServerName(HostName hn) { _serverHostName = hn; }
    void setServerPort(uint32_t p) { _serverPort = p; }
    void setConference(Conference* conf) { _conf = conf; }

    // ----- From Authority ---------------------------------------------------

    virtual void validate(StationID id);

    // ----- From IPLibEvents -------------------------------------------------

    virtual void reset() { }
    virtual void dns(HostName name, IPAddress addr);
    virtual void bind(Channel ch) { }
    virtual void conn(Channel ch);
    virtual void disc(Channel ch);
    virtual void recv(Channel ch, const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
        uint16_t fromPort);
    virtual void err(Channel ch, int type) { }

    // ----- From StateMachine2 -----------------------------------------------

protected:

    virtual void _process(int state, bool entry);

private:

    enum State { 
        IDLE, 
        REQUEST,
        DNS_WAIT, 
        CONNECT_WAIT, 
        DISCONNECT_WAIT, 
        FAILED, 
        SUCCEEDED 
    };

    IPLib* _ctx;
    UserInfo* _userInfo;
    Log* _log;
    Conference* _conf;

    HostName _serverHostName;
    uint32_t _serverPort;
    CallSign _targetCallSign;
    IPAddress _targetAddr;
    Channel _channel;
    // A place to accumulate characters while trying to build a complete 
    // directory entry.
    static const uint32_t _saveAreaSize = 256;
    uint8_t _saveArea[_saveAreaSize];
    uint32_t _saveAreaUsed;
};

}

#endif

