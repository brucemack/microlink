#ifndef _TestUserInfo_h
#define _TestUserInfo_h

#include <iostream>

#include "kc1fsz-tools/AudioOutputContext.h"

#include "../src/UserInfo.h"

namespace kc1fsz {

class TestUserInfo : public UserInfo {
public:

    void setAudioOut(AudioOutputContext* o) { _audioOutCtx = o; }

    virtual void setStatus(const char* msg) { std::cout << "UserInfo(Status): " << msg << std::endl; }
    virtual void setOnData(const char* msg) { std::cout << "UserInfo(oNDATA): [" << msg << "]" << std::endl; }

    virtual void setSquelchOpen(bool sq) { 

        bool unkey = _squelch == true && sq == false;
        _squelch = sq;

        std:: cout << "UserInfo: Squelch: " << _squelch << std::endl;

        // Short beep on unkey
        if (unkey) {
            if (_audioOutCtx != 0) {
                _audioOutCtx->tone(400, 75);
            }
        }
    }

    bool getSquelch() const { return _squelch; }

private:

    bool _squelch = false;
    AudioOutputContext* _audioOutCtx = 0;
};

}

#endif
