#include <cassert>
#include <cstring>
#include <iostream>

#include "events/TickEvent.h"
#include "machines/RootMachine.h"
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
    
    cout << "Start" << endl;
    // 1. Trigger DNS
    rm.start(&context);

    cout << "Cycle 1" << endl;
    // 1. The completion of the DNS 
    // 2. Generate the TCP open
    // 3. Start the TCP connect
    context.applyEvents(&rm);

    cout << "Cycle 2" << endl;
    // 1. Complete TCP connect
    // 2. Send the logon message to the server
    context.applyEvents(&rm);

    cout << "Cycle 3" << endl;
    // 1. Get data back from the server
    context.applyEvents(&rm);

    cout << "Cycle 4" << endl;
    // 1. Get disconnect
    context.applyEvents(&rm);

    cout << "Cycle 5" << endl;
    context.applyEvents(&rm);

    // Still working
    assert(!rm.isDone());
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
    
    cout << "Start" << endl;
    // 1. Trigger DNS
    rm.start(&context);

    cout << "Cycle 1" << endl;
    // 1. The completion of the DNS 
    // 2. Generate the TCP open
    // 3. Start the TCP connect
    context.applyEvents(&rm);

    cout << "Cycle 2" << endl;
    // 1. Complete TCP connect
    // 2. Send the logon message to the server
    context.applyEvents(&rm);

    cout << "Cycle 3a" << endl;
    // 1. Send in a tick and see nothing happen
    TickEvent tick;
    rm.processEvent(&tick, &context);
    assert(!rm.isDone());

    // Move the time forward 20 seconds so that we force
    // a timeout
    context.setTimeMs(1000 + 20000);

    cout << "Cycle 3b" << endl;
    // 1. Send in a tick and watch the timeout get triggered
    rm.processEvent(&tick, &context);

    assert(rm.isDone());
    assert(!rm.isGood());
}

int main(int, const char**) {
    misc_test_1();
    //machine_test_1();
    machine_test_2();
    return 0;
}
