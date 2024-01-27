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
#include "Event.h"

namespace kc1fsz {

template<typename C> class StateMachine {
public:

    virtual void processEvent(const Event* event, C* context) = 0;
    virtual void start(C* context) = 0;
    virtual bool isDone() const = 0;
    virtual bool isGood() const = 0;

protected:

    bool isDoneAfterEvent(StateMachine& mach, const Event* ev, C* ctx) {
        mach.processEvent(ev, ctx);
        return mach.isDone();
    }

    void _setTimeoutMs(uint32_t t) {
        _timeoutTargetMs = t;
    }

    bool _isTimedOut(C* ctx) const {
        return (ctx->getTimeMs() > _timeoutTargetMs);
    }

private:

    uint32_t _timeoutTargetMs;
};

}

#endif
