#ifndef _TestContext_h
#define _TestContext_h

#include "../src/Context.h"

namespace kc1fsz {

class TestContext : public Context {
public:

    TestContext();

    void applyEvents(StateMachine<Context>* machine);

    virtual uint32_t getTimeMs() { return _timeMs; }

    TCPChannel createTCPChannel();
    void connectTCPChannel(TCPChannel c, IPAddress ipAddr);
    void sendTCPChannel(TCPChannel c, const uint8_t* b, uint16_t len);

    void startDNSLookup(HostName hostName);

    void setTimeMs(uint32_t ms) { _timeMs = ms; }

private:

    uint32_t _timeMs;

    bool _dnsWaiting;
    IPAddress _dnsResult;

    bool _connectWaiting;
    TCPChannel _connectResult;

    bool _disconnectWaiting;
    TCPChannel _disconnectResult;

    bool _tcpDataWaiting;
    uint8_t _tcpDataResult[256];
    uint32_t _tcpDataResultLen;
};

}

#endif
