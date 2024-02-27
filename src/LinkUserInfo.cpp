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
#include "LinkUserInfo.h"

using namespace std;

namespace kc1fsz {

void LinkUserInfo::setStatus(const char* msg) { 
    if (_log) {
        char stamp[16];
        snprintf(stamp, 16, "%06lu", time_ms() % 1000000);
        std::cout << "UserInfo(Status): " << stamp << " " << msg << std::endl; 
    }
}

void LinkUserInfo::setOnData(const char* msg) { 
    if (_log) {
        char stamp[16];
        snprintf(stamp, 16, "%06lu", time_ms() % 1000000);
        //std::cout << "UserInfo(oNDATA): " << stamp << "[" << msg << "]" << std::endl; 
        std::cout << "UserInfo(oNDATA): " << stamp << std::endl; 
    }
}

void LinkUserInfo::setSquelchOpen(bool sq) { 

    bool unkey = _squelch == true && sq == false;

    _squelch = sq;

    if (unkey) {
        _lastSquelchCloseTime = time_ms();
    }

    if (_log)
        std:: cout << "UserInfo: Squelch: " << _squelch << std::endl;
}

bool LinkUserInfo::getSquelch() const { 
    return _squelch; 
}

uint32_t LinkUserInfo::getMsSinceLastSquelchClose() const { 
    return time_ms() - _lastSquelchCloseTime; 
}

}
