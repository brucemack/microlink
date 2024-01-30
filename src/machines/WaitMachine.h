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
#ifndef _WaitMachine_h
#define _WaitMachine_h

#include "kc1fsz-tools/Event.h"

#include "../StateMachine.h"

namespace kc1fsz {

class WaitMachine : public StateMachine {
public:

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setTargetTimeMs(uint32_t targetTime);

private:

    enum State { IDLE, OPEN } _state;
    uint32_t _targetTime;
};

}

#endif