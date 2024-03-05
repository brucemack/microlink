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

//static const uint32_t ACCEPT_TIMEOUT_MS = 5 * 60 * 1000;
static const uint32_t ACCEPT_TIMEOUT_MS = 30 * 1000;

// This is how long the radio needs to be silent before we will start
// using the welcome message.
static const uint32_t QUIET_INTERVAL_MS = 2 * 60 * 1000;

LinkRootMachine::LinkRootMachine(CommContext* ctx, UserInfo* userInfo, 
    AudioOutputContext* audioOutput) 
:   _ctx(ctx),
    _userInfo(userInfo),
    _logonMachine(ctx, userInfo),
    _acceptMachine(ctx, userInfo),
    _validationMachine(ctx, userInfo),
    _welcomeMachine(ctx, userInfo, audioOutput),
    _qsoMachine(ctx, userInfo, audioOutput),
    _lookupMachine(ctx, userInfo),
    _connectMachine(ctx, userInfo) {
}

bool LinkRootMachine::run() {
    processEvent(&tickEv);
    return true;
}

void LinkRootMachine::start() {
    _state = State::IDLE;
    _stateCount = 0;
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
            _state = State::LOGON;
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
                _lastRemoteCallSign = _acceptMachine.getRemoteCallSign();
                _lastRemoteAddress = _acceptMachine.getRemoteAddress();
                _validationMachine.start();
                _state = State::IN_VALIDATION;
            } else {
                _userInfo->setStatus("Accept failed");
                // Close channels used for accepting
                _ctx->closeUDPChannel(_acceptMachine.getRTCPChannel());
                _ctx->closeUDPChannel(_acceptMachine.getRTPChannel());
                // Back to square 1
                _state = State::IDLE;
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus("Refreshing login");
            // Close channels used for accepting
            _ctx->closeUDPChannel(_acceptMachine.getRTCPChannel());
            _ctx->closeUDPChannel(_acceptMachine.getRTPChannel());
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
                // Close channels used for accepting
                _ctx->closeUDPChannel(_acceptMachine.getRTCPChannel());
                _ctx->closeUDPChannel(_acceptMachine.getRTPChannel());
                // Back to square 1
                _state = State::IDLE;
            }
        }
    }
    else if (_state == State::IN_WELCOME) {
        if (isDoneAfterEvent(_welcomeMachine, ev)) {
            if (_welcomeMachine.isGood()) {
                _qsoMachine.start();
                _state = State::QSO;
            } 
            else {
                // Close channels used for accepting
                _ctx->closeUDPChannel(_acceptMachine.getRTCPChannel());
                _ctx->closeUDPChannel(_acceptMachine.getRTPChannel());
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
    // In this state we are looking up the address of a node that 
    // the radio has requested to connect to
    else if (_state == State::LOOKUP) {
        if (isDoneAfterEvent(_lookupMachine, ev)) {
            if (_lookupMachine.isGood()) {
                // Move the address across
                _connectMachine.setTargetAddress(_lookupMachine.getTargetAddress());
                _qsoMachine.setPeerAddress(_lookupMachine.getTargetAddress());
                _connectMachine.start();
                _lastRemoteAddress = _lookupMachine.getTargetAddress();
                _state = State::CONNECT;
                // Number of connect tries
                _stateCount = 5;
            }
            else {
                _userInfo->setStatus("Lookup failed");
                // If the lookup fails we go reset and go back 
                // to square one.
                _state = State::IDLE;
            }
        }
    }
    // In this state we are looking up the address of a node that 
    // the radio has requested to connect to
    else if (_state == State::CONNECT) {
        if (isDoneAfterEvent(_connectMachine, ev)) {
            if (_connectMachine.isGood()) {
                // The connect process establishes UDP communication paths, 
                // so transfer them over to the QSO machine.
                _qsoMachine.setRTCPChannel(_connectMachine.getRTCPChannel());
                _qsoMachine.setRTPChannel(_connectMachine.getRTPChannel());
                _qsoMachine.setSSRC(_connectMachine.getSSRC());
                _qsoMachine.start();
                _state = State::QSO;
            } 
            // If the connection fails then retry it a few times
            else {
                if (_stateCount-- > 0) {
                    _connectRetryWaitMachine.setTargetTimeMs(time_ms() + 500);
                    _connectRetryWaitMachine.start();
                    _state = State::CONNECT_RETRY_WAIT;
                } else {
                    _userInfo->setStatus("Connect failed");
                    _ctx->closeUDPChannel(_connectMachine.getRTCPChannel());
                    _ctx->closeUDPChannel(_connectMachine.getRTPChannel());
                    // If the lookup fails we go reset and go back 
                    // to square one.
                    _state = State::IDLE;
                }
            }
        }
    }
    // In this state we are waiting for a brief period before going back 
    // to retry the connection.
    else if (_state == State::CONNECT_RETRY_WAIT) {
        if (isDoneAfterEvent(_connectRetryWaitMachine, ev)) {
            _connectMachine.start();
            _state = State::CONNECT;
        }
    }
}

bool LinkRootMachine::play(const int16_t* frame, uint32_t frameLen) {
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
    _lookupMachine.setServerName(h);
}

void LinkRootMachine::setServerPort(uint32_t p) {
    _logonMachine.setServerPort(p);
    _validationMachine.setServerPort(p);
    _lookupMachine.setServerPort(p);
}

void LinkRootMachine::setCallSign(CallSign cs) {
    _logonMachine.setCallSign(cs);
    _qsoMachine.setCallSign(cs);
    _connectMachine.setCallSign(cs);
}

void LinkRootMachine::setPassword(FixedString s) {
    _logonMachine.setPassword(s);
}

void LinkRootMachine::setFullName(FixedString n) {
    _connectMachine.setFullName(n);
    _qsoMachine.setFullName(n);
    _connectMachine.setFullName(n);
}

void LinkRootMachine::setLocation(FixedString loc) { 
    _logonMachine.setLocation(loc); 
    _qsoMachine.setLocation(loc);
    _connectMachine.setLocation(loc);
}

bool LinkRootMachine::requestCleanStop() {
    if (_state == State::QSO) {
        return _qsoMachine.requestCleanStop();
    } else {
        return false;
    }
}

bool LinkRootMachine::connectToStation(CallSign targetCs) {
    if (_state != State::ACCEPTING) {
        return false;
    }

    char buf[64];
    snprintf(buf, 64, "Connecting to %s", targetCs.c_str());
    _userInfo->setStatus(buf);

    _lookupMachine.setTargetCallSign(targetCs);
    _lookupMachine.start();
    _lastRemoteCallSign = targetCs;

    _state = State::LOOKUP;

    return true;
}


}
