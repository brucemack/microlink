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
#ifndef _LookupMachine2_h
#define _LookupMachine2_h

#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/CallSign.h"

#include "../StateMachine.h"

namespace kc1fsz {

class CommContext;
class UserInfo;

/**
 * A state machine used for managing the EL directory lookup.
*/
class LookupMachine2 : public StateMachine {
public:

    LookupMachine2(CommContext* ctx, UserInfo* userInfo);

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual void cleanup();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setServerName(HostName hn) { _serverHostName = hn; }
    void setServerPort(uint32_t p) { _serverPort = p; }
    void setTargetCallSign(CallSign cs) { _targetCallSign = cs; }
    IPAddress getTargetAddress() const { return _targetAddr; }

private:

    enum State { IDLE, DNS_WAIT, CONNECTING, WAITING_FOR_DISCONNECT, 
        FAILED, SUCCEEDED } _state;

    CommContext* _ctx;
    UserInfo* _userInfo;

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
