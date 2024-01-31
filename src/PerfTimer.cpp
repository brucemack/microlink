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
#include "PerfTimer.h"
#ifdef __CYGWIN__
#include <Windows.h>
#endif

namespace kc1fsz {

PerfTimer::PerfTimer() {
    QueryPerformanceFrequency(&_freq); 
    reset();
}

void PerfTimer::reset() {
    LARGE_INTEGER pc1;
    QueryPerformanceCounter(&pc1);
    _startUs = pc1.QuadPart * 1000000 / _freq.QuadPart;
}

uint32_t PerfTimer::elapsedUs() const {
    LARGE_INTEGER pc1;
    QueryPerformanceCounter(&pc1);
    uint32_t nowUs = pc1.QuadPart * 1000000 / _freq.QuadPart;
    return nowUs - _startUs;
}

}

