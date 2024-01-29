#include <iostream>
#include "TestContext.h"

#include "../src/events/DNSLookupEvent.h"
#include "../src/events/TCPConnectEvent.h"
#include "../src/events/TCPDisconnectEvent.h"
#include "../src/events/TCPReceiveEvent.h"
#include "../src/Channel.h"

using namespace std;

namespace kc1fsz {

Channel TestContext::createTCPChannel() {
    cout << "TestContext: Asked for TCP channel" << endl;
    return channel0;
}

void TestContext::connectTCPChannel(Channel c, IPAddress a, uint32_t port) {
    cout << "TestContext: Asked for TCP connection to " << a.getAddr() << " " << port << endl;;
    channel0 = c;
    ipAddr = a;
}

void TestContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {
    cout << "TestContext: TCP send request: " << endl;
    prettyHexDump(b, len, cout);
    channel0 = c;
    memcpyLimited(data, b, len, 256);
}

void TestContext::startDNSLookup(HostName hn) {
    cout << "TestContext: Requested DNS lookup: [" << hn.c_str() << "]" << endl;
    hostName = hn;
}

Channel TestContext::createUDPChannel(uint32_t localPort) {
    cout << "TestContext: Asked for UDP channel port " << localPort << endl;
    Channel r = channel0;
    channel0 = channel1;
    return r;
}

void TestContext::sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
    const uint8_t* b, uint16_t len) {
    cout << "TestContext: UDP send request to port " << targetPort << endl;
    prettyHexDump(b, len, cout);
    channel0 = c;
    memcpyLimited(data, b, len, 256);
}
      


}
