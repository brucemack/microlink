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

void QSOFlowMachine::setRTCPChannel(Channel c) {
    _rtcpChannel = c;
}

void QSOFlowMachine::setRTPChannel(Channel c) {
        _rtcpChannel = c;
}

}


