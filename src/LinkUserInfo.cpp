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
#include "kc1fsz-tools/Common.h"
#include "LinkUserInfo.h"

using namespace std;

namespace kc1fsz {

void LinkUserInfo::setStatus(const char* msg) { 
    if (_log)
        _log->info("Status: %s", msg);
}

void LinkUserInfo::setOnData(const char* msg) { 
    if (_log) {
        //std::cout << "UserInfo(oNDATA): " << stamp << "[" << msg << "]" << std::endl; 
        _log->info("oNDATA");
    }
}

void LinkUserInfo::setSquelchOpen(bool sq) { 

    bool unkey = _squelch == true && sq == false;

    _squelch = sq;

    if (unkey) {
        _lastSquelchCloseTime = time_ms();
    }

    if (_log)
        _log->info("Squelch: %d", _squelch);
}

bool LinkUserInfo::getSquelch() const { 
    return _squelch; 
}

uint32_t LinkUserInfo::getMsSinceLastSquelchClose() const { 
    return ms_since(_lastSquelchCloseTime); 
}

}
