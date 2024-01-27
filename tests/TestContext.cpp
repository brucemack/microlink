#include <iostream>
#include "TestContext.h"

#include "../src/events/DNSLookupEvent.h"
#include "../src/events/TCPConnectEvent.h"
#include "../src/events/TCPDisconnectEvent.h"
#include "../src/events/TCPReceiveEvent.h"

using namespace std;

namespace kc1fsz {

TCPChannel TestContext::createTCPChannel() {
    cout << "Asked for channel" << endl;
    return channel;
}

void TestContext::connectTCPChannel(TCPChannel c, IPAddress a) {
    cout << "Asked for connection" << endl;
    channel = c;
    ipAddr = a;
}

void TestContext::sendTCPChannel(TCPChannel c, const uint8_t* b, uint16_t len) {
    cout << "TCP send request: " << endl;
    prettyHexDump(b, len, cout);
    channel = c;
    memcpyLimited(data, b, len, 256);
}

void TestContext::startDNSLookup(HostName hn) {
    cout << "Requested DNS lookup: [" << hn.c_str() << "]" << endl;
    hostName = hn;
}

}
