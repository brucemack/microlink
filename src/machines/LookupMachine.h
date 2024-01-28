#ifndef _LookupMachine_h
#define _LookupMachine_h

#include "../Event.h"
#include "../Context.h"
#include "../StateMachine.h"
#include "../IPAddress.h"
#include "../HostName.h"
#include "../CallSign.h"

namespace kc1fsz {

/**
 * A state machine used for managing the EL directory lookup.
*/
class LookupMachine : public StateMachine<Context> {
public:

    LookupMachine() : _state(IDLE) { }

    virtual void processEvent(const Event* ev, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setServerName(HostName hn) { _serverHostName = hn; }
    void setTargetCallSign(CallSign cs) { _targetCallSign = cs; }
    IPAddress getTargetAddress() const { return _targetAddr; }

private:

    enum State { IDLE, DNS_WAIT, CONNECTING, WAITING_FOR_DISCONNECT, 
        FAILED, SUCCEEDED } _state;

    HostName _serverHostName;
    CallSign _targetCallSign;
    IPAddress _targetAddr;
    bool _foundTarget;

    Channel _channel;

    bool _headerSeen;
    // A place to accumulate characters while trying to 
    // build a complete directory entry.
    uint8_t _saveArea[64];
    uint32_t _saveAreaPtr;
};

}

#endif

