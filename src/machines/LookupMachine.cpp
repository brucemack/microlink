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
#include "LookupMachine.h"

namespace kc1fsz {

void LookupMachine::processEvent(const Event* ev, Context* context) {
}

void LookupMachine::start(Context* ctx) {
}

bool LookupMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool LookupMachine::isGood() const {
    return 
    _state == SUCCEEDED;
}

void LookupMachine::setServerName(HostName h) {
    _serverHostName = h;
}

void LookupMachine::setTargetCallSign(CallSign cs) {
    _targetCallSign = cs;
}

IPAddress LookupMachine::getTargetAddress() const {
    return _targetAddress;
}

}
