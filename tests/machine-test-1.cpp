#include <cassert>
#include <cstring>
#include <iostream>

#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/events/TCPDisconnectEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"

#include "../src/machines/RootMachine.h"
#include "../src/machines/LookupMachine2.h"

#include "TestContext.h"
#include "TestUserInfo.h"
#include "TestAudioOutputContext.h"

using namespace std;
using namespace kc1fsz;

static void misc_test_1() {
    CallSign cs("KC1FSZ");
    assert(strcmp("KC1FSZ", cs.c_str()) == 0);

    CallSign cs2("0123456789012345678901234567890123456789");
    assert(strcmp("0123456789012345678901234567890", cs2.c_str()) == 0);
}

static void machine_test_1() {

    set_time_ms(1000);

    TestContext context;    
    TestUserInfo info;
    TestAudioOutputContext audioOutput(0, 8000);

    RootMachine rm(&context, &info, &audioOutput);
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setServerPort(5200);
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString("XYZ123"));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    rm.setTargetCallSign(CallSign("KC1FSZ"));
    
    {
        cout << "--- Start" << endl;
        // 1. See DNS request
        rm.start();
        assert(strcmp(context.hostName.c_str(), "naeast.echolink.org") == 0);
    }

    {
        cout << "--- Cycle 1" << endl;
        // 1. Generate the completion of the DNS 
        // 2. See the TCP open
        // 3. See the TCP connect
        context.channel0 = Channel(2);
        DNSLookupEvent ev(HostName("a.com"), IPAddress(8));
        rm.processEvent(&ev);
        assert(context.channel0.getId() == 2);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 2" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(Channel(2));
        rm.processEvent(&ev);
    }

    {
        cout << "--- Cycle 3" << endl;
        // 1. Generate data back from the server
        // 2. Generate the disconnect
        TCPReceiveEvent ev(Channel(2), (const uint8_t*)"OK2.6", 5);
        rm.processEvent(&ev);
        context.hostName = HostName("DUMMY");
        TCPDisconnectEvent ev2(Channel(2));
        rm.processEvent(&ev2);
        // See that the lookup has started (DNS request)
        assert(strcmp(context.hostName.c_str(), "naeast.echolink.org") == 0);
    }

    // Still working
    assert(!rm.isDone());

    {
        cout << "--- Cycle 4" << endl;
        // 1. Generate DNS response
        context.channel0 = Channel(3);
        DNSLookupEvent ev(HostName("a.com"), IPAddress(8));
        rm.processEvent(&ev);
        assert(context.channel0.getId() == 3);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 5" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(Channel(3));
        rm.processEvent(&ev);
    }
    
    {
        cout << "--- Cycle 6" << endl;
        // 1. Generate data back from the server in a few parts

        // NOTE: These message parts are used for the "s" command but not the "v" command
        //TCPReceiveEvent ev(Channel(3), (const uint8_t*)"@@@\n1111\n", 9);
        //rm.processEvent(&ev);
        //TCPReceiveEvent ev2(Channel(3), (const uint8_t*)"XXXX", 4);
        //rm.processEvent(&ev2);

        TCPReceiveEvent ev3(Channel(3), (const uint8_t*)"\n1\n2\n1.2.3.4\nKC1", 15 + 1);
        rm.processEvent(&ev3);
        //TCPReceiveEvent ev4(Channel(3), (const uint8_t*)"FSZ\n1\n2\n3\n0.0.1.255\n", 20);
        TCPReceiveEvent ev4(Channel(3), (const uint8_t*)"FSZ\n1\n2\n0.0.1.255\n", 18);
        rm.processEvent(&ev4);
    }

    {
        cout << "--- Cycle 7" << endl;
        // Get the two UDP connections ready
        // RTP is setup first, RTCP second
        context.channel0 = Channel(4);
        context.channel1 = Channel(5);
        // Simulate the disconnect from the server.  This should trigger the completion
        // of the lookup and will start the QSO connect.
        TCPDisconnectEvent ev5(Channel(3));
        rm.processEvent(&ev5);
        // Generate oNDATA packet back from the other peer.  
        uint8_t buf[3] = { 0xc0, 0xc9, 0 };
        UDPReceiveEvent ev4(Channel(5), (const uint8_t*)buf, 3);
        rm.processEvent(&ev4);
    }    

    {
        cout << "--- Cycle 8" << endl;
        TickEvent ev;
        rm.processEvent(&ev);
        // Generate some audio traffic
        // oNDATA Message
        UDPReceiveEvent ev4(Channel(4), (const uint8_t*)"oNDATA\rHello World!\rTest\r", 25);
        rm.processEvent(&ev4);

    }

    {
        cout << "--- Cycle 9a" << endl;
        // Move the time forward 10 seconds so that we can see the keep-alive message
        advance_time_ms(10000);
        TickEvent ev;
        rm.processEvent(&ev);
        cout << "--- Cycle 9b" << endl;
        // Move the time forward 5 seconds 
        advance_time_ms(5001);
        rm.processEvent(&ev);
        cout << "--- Cycle 9c" << endl;
        // Move the time forward 5 seconds 
        advance_time_ms(5001);
        rm.processEvent(&ev);
        cout << "--- Cycle 9d" << endl;
        // Move the time forward 20 seconds - this should trigger an end
        advance_time_ms(20000);
        rm.processEvent(&ev);
    }

    // We get all the way to the end
    assert(rm.isDone());
    assert(rm.isGood());
}

// This is a test of a timeout during logon
//
static void machine_test_2() {

    set_time_ms(1000);

    TestContext context;
    TestUserInfo info;
    TestAudioOutputContext audioOutput(0, 8000);

    RootMachine rm(&context, &info, &audioOutput);
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString("XYZ123"));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    
    {
        cout << "--- Start" << endl;
        // 1. See DNS request
        rm.start();
        assert(strcmp(context.hostName.c_str(), "naeast.echolink.org") == 0);
    }

    {
        cout << "--- Cycle 1" << endl;
        // 1. Generate the completion of the DNS 
        // 2. See the TCP open
        // 3. See the TCP connect
        context.channel0 = Channel(2);
        DNSLookupEvent ev(HostName("a.com"), IPAddress(8));
        rm.processEvent(&ev);
        assert(context.channel0.getId() == 2);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 2" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(Channel(2));
        rm.processEvent(&ev);
    }

    {
        cout << "--- Cycle 3" << endl;
        // 1. Generate data back from the server
        // 2. Generate the disconnect
        TCPReceiveEvent ev(Channel(2), (const uint8_t*)"OK2.6", 5);
        rm.processEvent(&ev);

        // Generate a tick
        TickEvent ev2;
        rm.processEvent(&ev2);

        // Validate that we're still working
        assert(!rm.isDone());

        // Move forward 20 seconds (past the timeout)
        advance_time_ms(20000);

        // Generate a tick
        rm.processEvent(&ev2);

        // Validate that we've failed
        assert(rm.isDone());
        assert(!rm.isGood());
    }
}

// New lookup 
static void machine_test_3() {

    cout << "Test 3" << endl;

    set_time_ms(1000);

    TestContext context;
    TestUserInfo info;
    TestAudioOutputContext audioOutput(0, 8000);

    LookupMachine2 lm(&context, &info);
    lm.setServerName(HostName("a.com"));
    lm.setTargetCallSign(CallSign("KC1FSZ"));

    {
        // 1. See DNS request
        lm.start();
        assert(strcmp(context.hostName.c_str(), "a.com") == 0);
    }

    {
        cout << "--- Cycle 1" << endl;
        // 1. Generate the completion of the DNS 
        // 2. See the TCP open
        // 3. See the TCP connect
        context.channel0 = Channel(2);
        DNSLookupEvent ev(HostName("a.com"), IPAddress(8));
        lm.processEvent(&ev);
        assert(context.channel0.getId() == 2);
        assert(context.ipAddr.getAddr() == 8);
    }

    {
        cout << "--- Cycle 2" << endl;
        // 1. Generate TCP connect complete
        // 2. See the logon message sent to the server
        TCPConnectEvent ev(Channel(2));
        lm.processEvent(&ev);
    }

    {
        cout << "--- Cycle 3" << endl;
        // 1. Generate data back from the server
        // 2. Generate the disconnect
        const char* msg = "A\nB\n1234\n1.2.3.4\n";
        TCPReceiveEvent ev(Channel(2), (const uint8_t*)msg, strlen(msg));
        lm.processEvent(&ev);
        // Simulate the disconnect from the server.  This should trigger the completion
        // of the lookup and will start the QSO connect.
        TCPDisconnectEvent ev5(Channel(2));
        lm.processEvent(&ev5);
    }
}

int main(int, const char**) {
    misc_test_1();
    machine_test_1();
    machine_test_2();
    machine_test_3();
    return 0;
}

