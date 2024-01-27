#include <cassert>
#include <cstring>
#include <iostream>

#include "../src/events/TickEvent.h"
#include "../src/machines/RootMachine.h"
#include "../src/events/DNSLookupEvent.h"
#include "../src/events/TCPConnectEvent.h"
#include "../src/events/TCPDisconnectEvent.h"
#include "../src/events/TCPReceiveEvent.h"

#include "TestContext.h"

using namespace std;
using namespace kc1fsz;

static void misc_test_1() {
    CallSign cs("KC1FSZ");
    assert(strcmp("KC1FSZ", cs.c_str()) == 0);

    CallSign cs2("0123456789012345678901234567890123456789");
    assert(strcmp("0123456789012345678901234567890", cs2.c_str()) == 0);
}

static void machine_test_1() {

    TestContext context;
    RootMachine rm;
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString("XYZ123"));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    rm.setTargetCallSign(CallSign("KC1FSZ"));
    
    {
        cout << "--- Start" << endl;
        // 1. See DNS request
        rm.start(&context);
        assert(strcmp(context.hostName.c_str(), "naeast.echolink.org") == 0);
    }

    {
        cout << "--- Cycle 1" << endl;
        // 1. Generate the completion of the DNS 
        // 2. See the TCP open
        // 3. See the TCP connect
        context.channel = TCPChannel(2);
        DNSLookupEvent ev(IPAddress(8));
        rm.processEvent(&ev, &context);
        assert(context.channel.getId() == 2);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 2" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(TCPChannel(2));
        rm.processEvent(&ev, &context);
    }

    {
        cout << "--- Cycle 3" << endl;
        // 1. Generate data back from the server
        // 2. Generate the disconnect
        TCPReceiveEvent ev(TCPChannel(2), (const uint8_t*)"OK2.6", 5);
        rm.processEvent(&ev, &context);
        context.hostName = HostName("DUMMY");
        TCPDisconnectEvent ev2(TCPChannel(2));
        rm.processEvent(&ev2, &context);
        // See that the lookup as started (DNS request)
        assert(strcmp(context.hostName.c_str(), "naeast.echolink.org") == 0);
    }

    // Still working
    assert(!rm.isDone());

    {
        cout << "--- Cycle 4" << endl;
        // 1. Generate DNS response
        context.channel = TCPChannel(3);
        DNSLookupEvent ev(IPAddress(8));
        rm.processEvent(&ev, &context);
        assert(context.channel.getId() == 3);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 5" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(TCPChannel(3));
        rm.processEvent(&ev, &context);
    }
    
    {
        cout << "--- Cycle 6" << endl;
        // 1. Generate data back from the server in a few parts
        TCPReceiveEvent ev(TCPChannel(3), (const uint8_t*)"@@@\n1111\n", 9);
        rm.processEvent(&ev, &context);
        TCPReceiveEvent ev2(TCPChannel(3), (const uint8_t*)"XXXX", 4);
        rm.processEvent(&ev2, &context);
        TCPReceiveEvent ev3(TCPChannel(3), (const uint8_t*)"\n1\n2\n3\n1.2.3.4\nKC1", 15 + 3);
        rm.processEvent(&ev3, &context);
        TCPReceiveEvent ev4(TCPChannel(3), (const uint8_t*)"FSZ\n1\n2\n3\n0.0.1.255\n", 20);
        rm.processEvent(&ev4, &context);
    }
    
}

// This is a test of a timeout during logon
//
static void machine_test_2() {

    TestContext context;
    context.setTimeMs(1000);

    RootMachine rm;
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString("XYZ123"));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    
    {
        cout << "--- Start" << endl;
        // 1. See DNS request
        rm.start(&context);
        assert(strcmp(context.hostName.c_str(), "naeast.echolink.org") == 0);
    }

    {
        cout << "--- Cycle 1" << endl;
        // 1. Generate the completion of the DNS 
        // 2. See the TCP open
        // 3. See the TCP connect
        context.channel = TCPChannel(2);
        DNSLookupEvent ev(IPAddress(8));
        rm.processEvent(&ev, &context);
        assert(context.channel.getId() == 2);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 2" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(TCPChannel(2));
        rm.processEvent(&ev, &context);
    }

    {
        cout << "--- Cycle 3" << endl;
        // 1. Generate data back from the server
        // 2. Generate the disconnect
        TCPReceiveEvent ev(TCPChannel(2), (const uint8_t*)"OK2.6", 5);
        rm.processEvent(&ev, &context);

        // Generate a tick
        TickEvent ev2;
        rm.processEvent(&ev2, &context);

        // Validate that we're still working
        assert(!rm.isDone());

        // Move forward 20 seconds (past the timeout)
        context.setTimeMs(1000 + 20000);

        // Generate a tick
        rm.processEvent(&ev2, &context);

        // Validate that we've failed
        assert(rm.isDone());
        assert(!rm.isGood());
    }
}


int main(int, const char**) {
    misc_test_1();
    machine_test_1();
    //machine_test_2();
    return 0;
}
