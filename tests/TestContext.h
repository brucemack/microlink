#ifndef _TestContext_h
#define _TestContext_h

#include "../src/Context.h"

namespace kc1fsz {

class TestContext : public Context {
public:

    virtual uint32_t getTimeMs() { return _timeMs; }

    void setTimeMs(uint32_t ms) { _timeMs = ms; }

    void advanceTimeMs(uint32_t ms) { _timeMs += ms; }

    Channel createTCPChannel();

    void connectTCPChannel(Channel c, IPAddress ipAddr);

    void sendTCPChannel(Channel c, const uint8_t* b, uint16_t len);

    Channel createUDPChannel(uint32_t localPort);

    void sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
        const uint8_t* b, uint16_t len);

    void startDNSLookup(HostName hostName);

    Channel channel0;
    Channel channel1;
    IPAddress ipAddr;
    HostName hostName;
    uint8_t data[256];
    uint32_t dataLen;

private:

    uint32_t _timeMs;
};

}

#endif
