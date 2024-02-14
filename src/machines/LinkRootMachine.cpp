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
#include "LinkRootMachine.h"

using namespace std;

namespace kc1fsz {

static TickEvent tickEv;

int LinkRootMachine::traceLevel = 0;

LinkRootMachine::LinkRootMachine(CommContext* ctx, UserInfo* userInfo, 
    AudioOutputContext* audioOutput) 
:   _state(IDLE),
    _ctx(ctx),
    _userInfo(userInfo),
    _logonMachine(ctx, userInfo),
    _acceptMachine(ctx, userInfo),
    _qsoMachine(ctx, userInfo, audioOutput) {
}

bool LinkRootMachine::isInQSO() const {
    return !(isDone() || _state == State::IDLE);
}

bool LinkRootMachine::run() {
    processEvent(&tickEv);
    return true;
}

void LinkRootMachine::start() {
    // Reset
    _ctx->reset();
    _state = State::IN_RESET;
}

void LinkRootMachine::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        if (ev->getType() != TickEvent::TYPE) {
            cout << "LinkRootMachine: state=" << _state << " event=" << ev->getType() << endl;
        }
    }

    // In this state we are doing nothing waiting to be started
    if (_state == State::IDLE) {
    }
    // In this state we are waiting for the full reset
    else if (_state == State::IN_RESET) {
        if (ev->getType() == StatusEvent::TYPE) {
            // TODO: ADD SOMETHING TO OPEN THE UDP CHANNELS
            _state = LOGON;
            // Start the login process
            _logonMachine.start();
        }
    }
    // TODO: ADD SOMETHING TO OPEN THE UDP CHANNELS

    // In this state we are waiting for the EL Server to process our 
    // logon request.
    else if (_state == LOGON) {
        if (isDoneAfterEvent(_logonMachine, ev)) {
            if (_logonMachine.isGood()) {
                // No data transfer is needed.  If we succeeded in the 
                // login then keep going.
                _acceptMachine.start();
                _state = State::ACCEPTING;
            } else {
                _userInfo->setStatus("Login failed");
                _state = State::FAILED;
            }
        }
    }
    else if (_state == ACCEPTING) {
        if (isDoneAfterEvent(_acceptMachine, ev)) {
            if (_acceptMachine.isGood()) {
                // No data transfer is needed.  If we succeeded in the 
                // login then keep going.
                //_valiationMachine.start();
                _state = State::IN_VALIDATION;
            } else {
                _userInfo->setStatus("Accept failed");
                _state = State::FAILED;
            }
        }
    }
    else if (_state == State::IN_VALIDATION) {

        // TODO: FILL IN!

        // The connect process establishes UDP communication paths, 
        // so transfer them over to the QSO machine.
        _qsoMachine.setRTCPChannel(_acceptMachine.getRTCPChannel());
        _qsoMachine.setRTPChannel(_acceptMachine.getRTPChannel());
        _qsoMachine.setSSRC(_acceptMachine.getSSRC());
        _qsoMachine.setTargetAddress(_acceptMachine.getRemoteAddress());
        _qsoMachine.start();
        _state = QSO;
    }
    // In this state a QSO is ongoing
    else if (_state == QSO) {
        if (isDoneAfterEvent(_qsoMachine, ev)) {
            _state = SUCCEEDED;
        }
    }
}

bool LinkRootMachine::play(const int16_t* frame) {
    if (_state == State::QSO) {
        return _qsoMachine.txAudio(frame);
    } else {
        return false;
    }
}

bool LinkRootMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool LinkRootMachine::isGood() const {
    return _state == SUCCEEDED;
}

void LinkRootMachine::setServerName(HostName h) {
    _logonMachine.setServerName(h);
    //_validationMachine.setServerName(h);
}

void LinkRootMachine::setServerPort(uint32_t p) {
    _logonMachine.setServerPort(p);
    //_validationMachine.setServerPort(p);
}

void LinkRootMachine::setCallSign(CallSign cs) {
    _logonMachine.setCallSign(cs);
    //_connectMachine.setCallSign(cs);
    _qsoMachine.setCallSign(cs);
}

void LinkRootMachine::setPassword(FixedString s) {
    _logonMachine.setPassword(s);
}

void LinkRootMachine::setFullName(FixedString n) {
    //_connectMachine.setFullName(n);
    _qsoMachine.setFullName(n);
}

void LinkRootMachine::setLocation(FixedString loc) { 
    _logonMachine.setLocation(loc); 
    //_connectMachine.setLocation(loc);
    _qsoMachine.setLocation(loc);
}

bool LinkRootMachine::requestCleanStop() {
    if (_state == State::QSO) {
        return _qsoMachine.requestCleanStop();
    } else {
        return false;
    }
}

}
