#ifndef _FixedString_h
#define _FixedString_h

#include "common.h"

namespace kc1fsz {

/**
 * IMPORTANT: Maximum length is 64 bytes!
 */
class FixedString {
public:

    FixedString() { _s[0] = 0; }
    FixedString(const FixedString& that) { strcpyLimited(_s, that._s, 64); }
    FixedString(const char* s) { strcpyLimited(_s, s, 64); }
    const char* c_str() { return _s; }
    uint32_t len() const { return std::strlen(_s); }

private:

    char _s[64];
};

}

#endif
