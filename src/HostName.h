#ifndef _HostName_h
#define _HostName_h

//#include "common.h"

namespace kc1fsz {

class HostName {
public:

    HostName() {
        _name[0] = 0;
    }

    HostName(const char* n) {
        setName(n);
    }

    void setName(const char* n) {
        //strcpyLimited(_name, n, 32);
    }

    const char* c_str() const {
        return _name;
    }

private:

    char _name[32];
};

}

#endif
