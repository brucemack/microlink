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
#include "machines/WelcomeMachine.h"

using namespace std;

namespace kc1fsz {

int WelcomeMachine::traceLevel = 0;

WelcomeMachine::WelcomeMachine(CommContext* ctx, UserInfo* userInfo, AudioProcessor* audioOut)
:   _ctx(ctx),
    _userInfo(userInfo) {
    _synth.setSink(audioOut);
}

void WelcomeMachine::start() {

    // Program the synth with the callsign welcome
    char msg[32];
    // Leading silence
    strcpy(msg,"__");
    // Callsign
    strcat(msg, _callSign.c_str());
    // Tack on "EchoLink Connect"
    strcat(msg, "_!@_");

    _synth.generate(msg);
    _state = State::PLAYING;
}

void WelcomeMachine::processEvent(const Event* ev) {

    if (_state == State::PLAYING) {

        // Make sure things are running at all times
        _synth.run();

        if (_synth.isFinished()) {
            if (traceLevel > 0) {
                cout << "Finished playing" << endl;
            }
            _state = State::SUCCEEDED;
        }
    }
}

void WelcomeMachine::cleanup() {
}

bool WelcomeMachine::isDone() const {
    return _state == State::SUCCEEDED;
}

bool WelcomeMachine::isGood() const {
    return _state == State::SUCCEEDED;
}

}

