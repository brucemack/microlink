#include <iostream>
#include "TestContext.h"

#include "../src/events/DNSLookupEvent.h"
#include "../src/events/TCPConnectEvent.h"
#include "../src/events/TCPDisconnectEvent.h"
#include "../src/events/TCPReceiveEvent.h"
#include "../src/UDPChannel.h"
#include "../src/TCPChannel.h"

using namespace std;

namespace kc1fsz {

TCPChannel TestContext::createTCPChannel() {
    cout << "Asked for TCP channel" << endl;
    return tcpChannel;
}

void TestContext::connectTCPChannel(TCPChannel c, IPAddress a) {
    cout << "Asked for connection" << endl;
    tcpChannel = c;
    ipAddr = a;
}

void TestContext::sendTCPChannel(TCPChannel c, const uint8_t* b, uint16_t len) {
    cout << "TCP send request: " << endl;
    prettyHexDump(b, len, cout);
    tcpChannel = c;
    memcpyLimited(data, b, len, 256);
}

void TestContext::startDNSLookup(HostName hn) {
    cout << "Requested DNS lookup: [" << hn.c_str() << "]" << endl;
    hostName = hn;
}

UDPChannel TestContext::createUDPChannel(uint32_t localPort) {
    cout << "Asked for UDP channel port " << localPort << endl;
    UDPChannel r = udpChannel0;
    udpChannel0 = udpChannel1;
    return r;
}

void TestContext::sendUDPChannel(UDPChannel c, IPAddress targetAddr, uint32_t targetPort, 
    const uint8_t* b, uint16_t len) {
    cout << "UDP send request to port " << targetPort << endl;
    prettyHexDump(b, len, cout);
}
        


}
