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
#ifndef _AtomicInteger_h
#define _AtomicInteger_h

#include "hardware/sync.h"

namespace kc1fsz {

// NOT SURE THIS IS RIGHT - STILL RESEARCHING
class AtomicInteger {
public:

    AtomicInteger() { _value = 0; }

    uint32_t get() const {
        __dsb();
        return _value;
    }

    void set(uint32_t v) {
        _value = v;
        __dsb();
    }

    // NOT SAFE FROM THE MULTI-WRITER CASE!
    void inc() {
        __dsb();
        _value++;
    }

private:

    uint32_t _value;
};

}

#endif

