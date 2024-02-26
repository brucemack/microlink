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
#ifndef _Synth_h
#define _Synth_h

#include <cstdint>

#include "kc1fsz-tools/Runnable.h"
#include "gsm-0610-codec/Decoder.h"

namespace kc1fsz {

class AudioProcessor;

class Synth : public Runnable {
public:

    Synth();

    void setSink(AudioProcessor* sink) { _sink = sink; }
    bool isFinished() const { return !_running; }
    void generate(const char* str);

    bool run();

private:

    AudioProcessor* _sink;

    Decoder _decoder;
    bool _running;

    static const uint32_t _strSize = 32;
    char _str[_strSize];
    uint32_t _strLen;
    uint32_t _strPtr;
    uint32_t _framePtr;

    bool _workingFrameReady;
    static const uint32_t _frameSize = 160 * 4;
    int16_t _workingFrame[_frameSize];
};

}

#endif
