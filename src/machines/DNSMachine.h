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
#ifndef _DNSMachine_h
#define _DNSMachine_h

#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/IPLib.h"

#include "../StateMachine2.h"

namespace kc1fsz {

class UserInfo;
class Log;

/**
 * This state machine is used to manage the process of 
 * periodically refreshing the IP address of an endpoint.
 */
class DNSMachine : public StateMachine2, public IPLibEvents {
public:

    static int traceLevel;

    DNSMachine(IPLib* ctx, UserInfo* userInfo, Log* log, 
        uint32_t intervalMs);

    void setHostName(HostName hn) { _hostName = hn; }
    bool isValid() const { return _isValid; }
    IPAddress getAddress() const { return _address; }

    // ----- From IPLibEvents -------------------------------------------------

    virtual void reset() { }
    virtual void dns(HostName name, IPAddress addr);
    virtual void bind(Channel ch) { }
    virtual void conn(Channel ch) { }
    virtual void disc(Channel ch) { }
    virtual void recv(Channel ch, const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
        uint16_t fromPort) { }
    virtual void err(Channel ch, int type) { }

    // ----- From StateMachine2 -----------------------------------------------

protected:

    virtual void _process(int state, bool entry);

private:

    enum State { 
        IDLE, 
        LINK_WAIT,
        DNS_WAIT, 
        WAIT,
        FAILED,
        SUCCEEDED,
    };

    IPLib* _ctx;
    UserInfo* _userInfo;
    Log* _log;
    uint32_t _intervalMs;
    
    HostName _hostName;
    bool _isValid;
    IPAddress _address;
};

}

#endif
