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

    static uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
        uint8_t* packet, uint32_t packetSize);

    static uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
        CallSign callSign, 
        FixedString fullName,
        uint32_t ssrc2,
        uint8_t* packet, uint32_t packetSize);      


    virtual void processEvent(const Event* ev, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString s) { _fullName = s; }
    void setLocation(FixedString l) { _location = l; }
    void setTargetAddress(IPAddress addr) { _targetAddr = addr; }

    UDPChannel getRTCPChannel() const;
    UDPChannel getRTPChannel() const;
    uint32_t getSSCR() const;

private:

    static uint32_t _ssrcCounter;

    enum State { IDLE, CONNECTING, SUCCEEDED, FAILED } _state;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    IPAddress _targetAddr;
    UDPChannel _rtpChannel;
    UDPChannel _rtcpChannel;
    uint32_t _ssrc;
};

}

#endif

