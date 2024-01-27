#ifndef _CallSign_h
#define _CallSign_h

#include <cstring>
#include "common.h"

namespace kc1fsz {

class CallSign {
public:

    CallSign() { _callSign[0] = 0; }
    CallSign(const CallSign& that) { strcpyLimited(_callSign, that._callSign, 32); }
    CallSign(const char* cs) { strcpyLimited(_callSign, cs, 32); }
    const char* c_str() { return _callSign; }
    uint32_t len() const { return std::strlen(_callSign); }
    bool operator== (const char* other) const { return strcmp(_callSign, other) == 0; }

private:

    char _callSign[32];
};

}

#endif
