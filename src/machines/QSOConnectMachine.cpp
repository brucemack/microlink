#include "../CallSign.h"
#include "QSOConnectMachine.h"

namespace kc1fsz {

void QSOConnectMachine::processEvent(const Event* ev, Context* context) {
}

void QSOConnectMachine::start(Context* ctx) {    
}

bool QSOConnectMachine::isDone() const {
    return false;
}

bool QSOConnectMachine::isGood() const {
    return false;
}

void QSOConnectMachine::setCallSign(CallSign cs) {
    _callSign = cs;
}

void QSOConnectMachine::setTargetAddress(IPAddress addr) {
    _targetAddr = addr;
}

UDPChannel QSOConnectMachine::getRTCPChannel() const {
    return _rtcpChannel;
}

UDPChannel QSOConnectMachine::getRTPChannel() const {
    return _rtpChannel;
}

}
