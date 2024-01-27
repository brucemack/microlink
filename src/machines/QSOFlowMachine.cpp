#include "../CallSign.h"
#include "../UDPChannel.h"
#include "QSOFlowMachine.h"

namespace kc1fsz {

void QSOFlowMachine::processEvent(const Event* ev, Context* context) {
}

void QSOFlowMachine::start(Context* ctx) {
}

bool QSOFlowMachine::isDone() const {
    return false;
}

bool QSOFlowMachine::isGood() const {
    return false;
}

void QSOFlowMachine::setCallSign(CallSign cs) {
    _callSign = cs;
}

void QSOFlowMachine::setRTCPChannel(UDPChannel c) {
    _rtcpChannel = c;
}

void QSOFlowMachine::setRTPChannel(UDPChannel c) {
        _rtcpChannel = c;
}

}


