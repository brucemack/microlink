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
#ifndef _LinkUserInfo_h
#define _LinkUserInfo_h

#include <iostream>

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/AudioOutputContext.h"

#include "../src/common.h"
#include "../src/UserInfo.h"

namespace kc1fsz {

class LinkUserInfo : public UserInfo {
public:

    void setLog(Log* l) { _log = l; }

    void setAudioOut(AudioOutputContext* o) { _audioOutCtx = o; }

    virtual void setStatus(const char* msg);

    virtual void setOnData(const char* msg);

    virtual void setSquelchOpen(bool sq);

    bool getSquelch() const;
    
    uint32_t getMsSinceLastSquelchClose() const;

private:

    Log* _log = 0;
    bool _squelch = false;
    AudioOutputContext* _audioOutCtx = 0;
    timestamp _lastSquelchCloseTime;
};

}

#endif
