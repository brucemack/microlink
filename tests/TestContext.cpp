#include <iostream>
#include "TestContext.h"

#include "../src/events/DNSLookupEvent.h"
#include "../src/events/TCPConnectEvent.h"
#include "../src/events/TCPDisconnectEvent.h"
#include "../src/events/TCPReceiveEvent.h"

using namespace std;

namespace kc1fsz {

TestContext::TestContext() {
    _dnsWaiting = false;
}

void TestContext::applyEvents(StateMachine<Context>* machine) {

    if (_dnsWaiting) {
        _dnsWaiting = false;
        DNSLookupEvent ev(_dnsResult);
        machine->processEvent(&ev, this);
    }
    else if (_connectWaiting) {
        _connectWaiting = false;
        TCPConnectEvent ev(_connectResult);
        machine->processEvent(&ev, this);
    }
    else if (_tcpDataWaiting) {
        _tcpDataWaiting = false;
        TCPReceiveEvent ev(_connectResult, _tcpDataResult, _tcpDataResultLen);
        machine->processEvent(&ev, this);
    }
    else if (_disconnectWaiting) {
        _disconnectWaiting = false;
        TCPDisconnectEvent ev(_connectResult);
        machine->processEvent(&ev, this);
    }
}

TCPChannel TestContext::createTCPChannel() {
    cout << "Asked for channel" << endl;
    return TCPChannel(1);
}

void TestContext::connectTCPChannel(TCPChannel c, IPAddress ipAddr) {
    cout << "Asked for connection" << endl;
    _connectWaiting = true;
    _connectResult = c;
}

void TestContext::sendTCPChannel(TCPChannel c, const uint8_t* b, uint16_t len) {

    cout << "TCP send request: " << endl;
    prettyHexDump(b, len, cout);

    // Assume this is a login in queue some text
    _tcpDataWaiting = true;
    _tcpDataResult[0] = 'O';
    _tcpDataResult[1] = 'K';
    _tcpDataResultLen = 2;

    // Assume this is a login in queue a response
    _disconnectWaiting = true;
    _disconnectResult = c;

}

void TestContext::startDNSLookup(HostName hostName) {
    _dnsWaiting = true;
    _dnsResult = IPAddress(8);
}

}
