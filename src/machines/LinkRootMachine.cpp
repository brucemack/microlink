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
#include "kc1fsz-tools/AudioOutputContext.h"
#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/StatusEvent.h"

#include "../UserInfo.h"
#include "LinkRootMachine.h"

using namespace std;

namespace kc1fsz {

static TickEvent tickEv;

int LinkRootMachine::traceLevel = 0;

// This is how long we wait to accept a new connection before going
// back through the login cycle.  This must be < the 7 minute period
// that EchoLink keeps the "ONLINE" status posted.

static const uint32_t ACCEPT_TIMEOUT_MS = 5 * 60 * 1000;

// This is how long the radio needs to be silent before we will start
// using the welcome message.
static const uint32_t QUIET_INTERVAL_MS = 2 * 60 * 1000;

LinkRootMachine::LinkRootMachine(CommContext* ctx, UserInfo* userInfo, 
    AudioOutputContext* audioOutput) 
:   _state(IDLE),
    _ctx(ctx),
    _userInfo(userInfo),
    _logonMachine(ctx, userInfo),
    _acceptMachine(ctx, userInfo),
    _validationMachine(ctx, userInfo),
    _welcomeMachine(ctx, userInfo, audioOutput),
    _qsoMachine(ctx, userInfo, audioOutput) {
}

bool LinkRootMachine::isInQSO() const {
    return _state == State::QSO;
}

bool LinkRootMachine::run() {
    processEvent(&tickEv);
    return true;
}

void LinkRootMachine::start() {
    _state = State::IDLE;
}

void LinkRootMachine::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        if (ev->getType() != TickEvent::TYPE) {
            cout << "LinkRootMachine: state=" << _state << " event=" << ev->getType() << endl;
        }
    }

    // In this state we are doing nothing waiting to be started
    if (_state == State::IDLE) {
        // Reset the communications channel
        _ctx->reset();
        _state = State::IN_RESET;
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
    // own logon request.
    else if (_state == LOGON) {
        if (isDoneAfterEvent(_logonMachine, ev)) {
            if (_logonMachine.isGood()) {
                // No data transfer is needed.  If we succeeded in the 
                // login then keep going.
                _acceptMachine.start();
                _state = State::ACCEPTING;
                _setTimeoutMs(time_ms() + ACCEPT_TIMEOUT_MS);
            } else {
                _userInfo->setStatus("Login failed");
                // Back to square 1
                _state = State::IDLE;
            }
        }
    }
    else if (_state == State::ACCEPTING) {
        if (isDoneAfterEvent(_acceptMachine, ev)) {
            if (_acceptMachine.isGood()) {
                // The connect process establishes the callsign
                _welcomeMachine.setCallSign(_acceptMachine.getRemoteCallSign());
                // The connect process establishes UDP communication paths, 
                // so transfer them over to the QSO machine.
                _qsoMachine.setRTCPChannel(_acceptMachine.getRTCPChannel());
                _qsoMachine.setRTPChannel(_acceptMachine.getRTPChannel());
                _qsoMachine.setSSRC(_acceptMachine.getSSRC());
                _qsoMachine.setPeerAddress(_acceptMachine.getRemoteAddress());
                // Pass information over to the validation machine
                _validationMachine.setRequestCallSign(_acceptMachine.getRemoteCallSign());
                _validationMachine.setRequestAddr(_acceptMachine.getRemoteAddress());
                _validationMachine.start();
                _state = State::IN_VALIDATION;
            } else {
                _userInfo->setStatus("Accept failed");
                // Back to square 1
                _state = State::IDLE;
            }
        }
        else if (_isTimedOut()) {
            // Back to square 1
            _state = State::IDLE;
        }
    }
    else if (_state == State::IN_VALIDATION) {
        if (isDoneAfterEvent(_validationMachine, ev)) {
            if (_validationMachine.isGood()) {
                // We only play the welcome message if the radio is quiet
                if ((time_ms() - _lastRadioCarrierDetect) > QUIET_INTERVAL_MS) {
                    _welcomeMachine.start();
                    _state = State::IN_WELCOME;
                } else {
                    _userInfo->setStatus("Skipping welcome, radio active");
                    _qsoMachine.start();
                    _state = State::QSO;
                }
            } 
            else {
                // Back to square 1
                _state = State::IDLE;
            }
        }
    }
    else if (_state == State::IN_WELCOME) {
        if (isDoneAfterEvent(_welcomeMachine, ev)) {
            if (_welcomeMachine.isGood()) {
                _qsoMachine.start();
                _state = QSO;
            } 
            else {
                // Back to square 1
                _state = State::IDLE;
            }
        }
    }
    // In this state a QSO is ongoing
    else if (_state == State::QSO) {
        if (isDoneAfterEvent(_qsoMachine, ev)) {
            // Once the converstaion ends (one way or the other)
            // we go reset and go back to square one.
            _state = State::IDLE;
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
    _validationMachine.setServerName(h);
}

void LinkRootMachine::setServerPort(uint32_t p) {
    _logonMachine.setServerPort(p);
    _validationMachine.setServerPort(p);
}

void LinkRootMachine::setCallSign(CallSign cs) {
    _logonMachine.setCallSign(cs);
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
