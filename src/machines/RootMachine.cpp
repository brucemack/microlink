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
#include "kc1fsz-tools/CommContext.h"
#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/StatusEvent.h"

#include "../UserInfo.h"
#include "RootMachine.h"

using namespace std;

namespace kc1fsz {

static TickEvent tickEv;

int RootMachine::traceLevel = 0;

RootMachine::RootMachine(CommContext* ctx, UserInfo* userInfo, 
    AudioOutputContext* audioOutput) 
:   _ctx(ctx),
    _userInfo(userInfo),
    _logonMachine(ctx, userInfo),
    _lookupMachine(ctx, userInfo),
    _connectMachine(ctx, userInfo),
    _qsoMachine(ctx, userInfo, audioOutput) {
}

bool RootMachine::isInQSO() const {
    return !(isDone() || _state == State::IDLE);
}

bool RootMachine::run() {
    processEvent(&tickEv);
    return true;
}

void RootMachine::start() {
    _ctx->reset();
    _state = State::IN_RESET;
}

void RootMachine::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        cout << "RootMachine: state=" << _state << " event=" << ev->getType() << endl;
    }

    // In this state we are doing nothing waiting to be started
    if (_state == State::IDLE) {
    }
    else if (_state == State::IN_RESET) {
        if (ev->getType() == StatusEvent::TYPE) {
            _state = LOGON;
            // Start the login process
            _logonMachine.start();
        }
    }
    // In this state we are waiting for the EL Server to process our 
    // logon request.
    else if (_state == LOGON) {
        if (isDoneAfterEvent(_logonMachine, ev)) {
            if (_logonMachine.isGood()) {
                // No data transfer is needed.  If we succeeded in the 
                // login then keep going.
                _lookupMachine.start();
                _state = State::LOOKUP;
            } else {
                _userInfo->setStatus("Login failed");
                _state = State::FAILED;
            }
        }
    }
    // In this state we are waiting for the EL Server to lookup the 
    // target callsign.
    else if (_state == State::LOOKUP) {
        if (isDoneAfterEvent(_lookupMachine, ev)) {
            if (_lookupMachine.isGood()) {
                // Transfer the target address that we got from the EL Server
                // into the connect machine and the QSO machine.
                _connectMachine.setTargetAddress(_lookupMachine.getTargetAddress());
                _qsoMachine.setPeerAddress(_lookupMachine.getTargetAddress());
                _connectMachine.start();
                _state = CONNECT; 
                // Number of connect tries
                _stateCount = 5;
            } else {
                _state = State::FAILED;
            }
        }
    }
    // In this state we are waiting for our QSO connection to be 
    // acknowledged by the 
    else if (_state == State::CONNECT) {
        if (isDoneAfterEvent(_connectMachine, ev)) {
            if (_connectMachine.isGood()) {
                // The connect process establishes UDP communication paths, 
                // so transfer them over to the QSO machine.
                _qsoMachine.setRTCPChannel(_connectMachine.getRTCPChannel());
                _qsoMachine.setRTPChannel(_connectMachine.getRTPChannel());
                _qsoMachine.setSSRC(_connectMachine.getSSRC());
                _qsoMachine.start();
                _state = QSO;
            } 
            // If the connection fails then retry it a few times
            else {
                if (_stateCount-- > 0) {
                    _connectRetryWaitMachine.setTargetTimeMs(time_ms() + 500);
                    _connectRetryWaitMachine.start();
                    _state = CONNECT_RETRY_WAIT;
                } else {
                    _state = FAILED;
                }
            }
        }
    }
    // In this state we are waiting for a brief period before going back 
    // to retry the connection.
    else if (_state == CONNECT_RETRY_WAIT) {
        if (isDoneAfterEvent(_connectRetryWaitMachine, ev)) {
            _connectMachine.start();
            _state = CONNECT;
        }
    }
    // In this state a QSO is ongoing
    else if (_state == QSO) {
        if (isDoneAfterEvent(_qsoMachine, ev)) {
            _state = SUCCEEDED;
        }
    }
}

bool RootMachine::play(const int16_t* frame) {
    if (_state == State::QSO) {
        return _qsoMachine.txAudio(frame);
    } else {
        return false;
    }
}

bool RootMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool RootMachine::isGood() const {
    return _state == SUCCEEDED;
}

void RootMachine::setServerName(HostName h) {
    _logonMachine.setServerName(h);
    _lookupMachine.setServerName(h);
}

void RootMachine::setServerPort(uint32_t p) {
    _logonMachine.setServerPort(p);
    _lookupMachine.setServerPort(p);
}

void RootMachine::setCallSign(CallSign cs) {
    _logonMachine.setCallSign(cs);
    _connectMachine.setCallSign(cs);
    _qsoMachine.setCallSign(cs);
}

void RootMachine::setTargetCallSign(CallSign cs) {
    _lookupMachine.setTargetCallSign(cs);
}

void RootMachine::setPassword(FixedString s) {
    _logonMachine.setPassword(s);
}

void RootMachine::setFullName(FixedString n) {
    _connectMachine.setFullName(n);
    _qsoMachine.setFullName(n);
}

void RootMachine::setLocation(FixedString loc) { 
    _logonMachine.setLocation(loc); 
    _connectMachine.setLocation(loc);
    _qsoMachine.setLocation(loc);
}

bool RootMachine::requestCleanStop() {
    if (_state == State::QSO) {
        return _qsoMachine.requestCleanStop();
    } else {
        return false;
    }
}

}
