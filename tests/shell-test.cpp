#include <stdio.h>
#include <iostream>

#include "pico/stdlib.h"

#include "kc1fsz-tools/OutStream.h"
#include "kc1fsz-tools/CommandShell.h"

using namespace std;
using namespace kc1fsz;

class TestOutStream : public OutStream {
public:

    virtual int write(uint8_t b) {
        cout.write((const char *)&b, 1);
        cout.flush();
        return 1;
    }

    virtual bool isWritable() const {
        return true;
    }
};

class TestCommandSink : public CommandSink {
public:

    virtual void process(const char* cmd) {
        cout << "PR: [" << cmd << "]" << endl;
    }
};

/*
Load command:

openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program shell-test.elf verify reset exit"

Minicom (for console, not the UART being tested):
minicom -b 115200 -o -D /dev/ttyACM0
*/
int main() {
 
    stdio_init_all();

    sleep_ms(2000);

    cout << "Shell Test" << endl;

    TestOutStream testOut;
    TestCommandSink testSink;
    CommandShell shell;
    shell.setOutput(&testOut);
    shell.setSink(&testSink);

    shell.reset();

    while (1) {
        int c = getchar_timeout_us(0);
        if (c > 0) {
            shell.process((char)c);
        }
    }
}
