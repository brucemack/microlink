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
#ifndef _StateMachine2_h
#define _StateMachine2_h

#include <cstdint>
#include <iostream>

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/Runnable.h"
#include "common.h"

namespace kc1fsz {

class StateMachine2 : public Runnable {
public:

    virtual ~StateMachine2() { }

    // ----- From Runnable ---------------------------------------------------

    virtual bool run() {
        if (_isTimedOut()) {
            //std::cout << "Timed out from " << _state << "->" << _timeoutState << std::endl;
            _setState(_timeoutState);
        }
        bool entry = _state != _lastState;
        _lastState = _state;
        _process(_state, entry);
        return true;
    }

protected:

    /**
     * This is where the real work should happen.
     * @param entry Indicates that this is the first call in a new state
     */
    virtual void _process(int state, bool entry) = 0;

    void _setState(int state) {        
        _state = state;
        _timeoutTargetMs = 0;
        _timeoutState = 0;
    }

    void _setState(int state, uint32_t timeoutMs, int timeoutState) {        
        _state = state;
        _timeoutTargetMs = time_ms() + timeoutMs;
        _timeoutState = timeoutState;
    }

    void _setTimeoutMs(uint32_t t) {
        _timeoutTargetMs = t;
    }

    bool _isTimedOut() const {
        return (_timeoutTargetMs != 0 && time_ms() > _timeoutTargetMs);
    }

    int _getState() const { return _state; }
    
    bool _isState(int state) { return _state == state; }

private:

    int _state = 0;
    int _lastState = 0;
    uint32_t _timeoutTargetMs = 0;
    int _timeoutState = 0;
};

}

#endif
