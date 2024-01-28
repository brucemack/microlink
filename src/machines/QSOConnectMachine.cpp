#include "../CallSign.h"
#include "QSOConnectMachine.h"

namespace kc1fsz {

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5198;

uint32_t QSOConnectMachine::_ssrcCounter = 0xf000;

void QSOConnectMachine::start(Context* ctx) {  
    // Get UDP connections created
    _rtpChannel = ctx->createUDPChannel(RTP_PORT);
    _rtcpChannel = ctx->createUDPChannel(RTCP_PORT);
    // Assign a unique SSRC
    _ssrc = _ssrcCounter++;
    // Make the initial messages and send




    ctx->startDNSLookup(_serverHostName);
    // We give the lookup 5 seconds to complete
    _setTimeoutMs(ctx->getTimeMs() + 5000);

    _state = CONNECTING;  
}

void QSOConnectMachine::processEvent(const Event* ev, Context* context) {
    if (_state == IDLE) {
    } 
    // In this state we are waiting for the reciprocal RTCP message
    else if (_state == CONNECTING) {
    }

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
