#ifndef _QSOFlowMachine_h
#define _QSOFlowMachine_h

#include "../Event.h"
#include "../Context.h"
#include "../StateMachine.h"
#include "../IPAddress.h"

namespace kc1fsz {

class QSOFlowMachine : public StateMachine<Event, Context> {
public:

    virtual void processEvent(const Event* ev, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs);
    void setRTCPChannel(UDPChannel c);
    void setRTPChannel(UDPChannel c);

private:

    enum State { IDLE, OPEN } _state;

    CallSign _callSign;
    UDPChannel _rtpChannel;
    UDPChannel _rtcpChannel;
};

}

#endif

