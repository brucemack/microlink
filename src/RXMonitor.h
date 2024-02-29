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
#ifndef _RXMonitor_h
#define _RXMonitor_h

#include <cstdint>

#include "kc1fsz-tools/AudioProcessor.h"
#include "kc1fsz-tools/Runnable.h"

namespace kc1fsz {

class UserInfo;

class RXMonitor : public AudioProcessor, public Runnable {
public:

    RXMonitor();

    void setSink(AudioProcessor* sink) { _sink = sink; }

    void setInfo(UserInfo* info) { _info = info; }

    void setDTMFDetector(AudioProcessor* sink) { _dtmfDet = sink; }

    /**
     * Controls whether the monitor should forward audio to the sink.
     */
    void setForward(bool keyed) { _keyed = keyed; }

    bool getForward() const { return _keyed; }

    // ----- From AudioProcessor -----------------------------------------------

    /**
     * @param frame 160 x 4 samples of 16-bit PCM audio.
     * @return true if the audio was taken, or false if the 
     *   session is busy and the TX will need to be 
     *   retried.
    */
    virtual bool play(const int16_t* frame, uint32_t frameLen);

    // ----- From Runnable ----------------------------------------------------

    virtual bool run();

private:

    bool _keyed = false;
    AudioProcessor* _sink = 0;
    UserInfo* _info = 0;
    AudioProcessor* _dtmfDet = 0;
};

}

#endif
