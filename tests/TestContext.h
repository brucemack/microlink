#ifndef _TestContext_h
#define _TestContext_h

#include "kc1fsz-tools/CommContext.h"

namespace kc1fsz {

class TestContext : public CommContext {
public:

    Channel createTCPChannel();

    void connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port);

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
};

}

#endif
