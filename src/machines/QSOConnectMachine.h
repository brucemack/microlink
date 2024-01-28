#ifndef _QSOConnectMachine_h
#define _QSOConnectMachine_h

#include "../Event.h"
#include "../Context.h"
#include "../StateMachine.h"
#include "../IPAddress.h"
#include "../UDPChannel.h"

namespace kc1fsz {

class QSOConnectMachine : public StateMachine<Context> {
public:

    virtual void processEvent(const Event* ev, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs);
    void setTargetAddress(IPAddress addr);

    UDPChannel getRTCPChannel() const;
    UDPChannel getRTPChannel() const;
    uint32_t getSSCR() const;

private:

    static uint32_t _ssrcCounter;

    enum State { IDLE, CONNECTING, SUCCEEDED, FAILED } _state;

    CallSign _callSign;
    IPAddress _targetAddr;
    UDPChannel _rtpChannel;
    UDPChannel _rtcpChannel;
    uint32_t _ssrc;
};

}

#endif

