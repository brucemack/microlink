#ifndef _IPAddress_h
#define _IPAddress_h

#include <cstdint>

namespace kc1fsz {

class IPAddress {
public:

    IPAddress() : _addr(0) { }
    IPAddress(const IPAddress& that) : _addr(that._addr) { }
    IPAddress(uint32_t addr) { _addr = addr; }
    uint32_t getAddr() const { return _addr; }

private:

    // IMPORTANT: This is stored in network byte order!
    uint32_t _addr;
};

}

#endif
