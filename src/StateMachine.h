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
#ifndef _StateMachine_h
#define _StateMachine_h

#include <cstdint>

#include "common.h"
#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/EventProcessor.h"

namespace kc1fsz {

class StateMachine : public EventProcessor {
public:

    virtual ~StateMachine() { }

    virtual void processEvent(const Event* event) = 0;
    virtual void start() = 0;
    virtual void cleanup() { };
    virtual bool isDone() const = 0;
    virtual bool isGood() const = 0;

protected:

    bool isDoneAfterEvent(StateMachine& mach, const Event* ev) {
        mach.processEvent(ev);
        bool done = mach.isDone();
        if (done) {
            mach.cleanup();
        }
        return done;
    }

    void _setState(int state, uint32_t timeoutMs) {        
        _state = state;
        _timeoutTargetMs = time_ms() + timeoutMs;
    }

    void _setTimeoutMs(uint32_t t) {
        _timeoutTargetMs = t;
    }

    bool _isTimedOut() const {
        return (time_ms() > _timeoutTargetMs);
    }

protected:

    int _state;

private:

    uint32_t _timeoutTargetMs;
};

}

#endif
