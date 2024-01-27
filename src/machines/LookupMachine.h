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

    void setServerName(HostName h);
    void setTargetCallSign(CallSign cs);
    IPAddress getTargetAddress() const;

private:

    enum State { IDLE, DNS_WAIT, CONNECT_WAIT, DISCONNECT_WAIT, FAILED, SUCCEEDED } _state;

    HostName _serverHostName;
    CallSign _targetCallSign;
    IPAddress _targetAddress;
};

}

#endif

