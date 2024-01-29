#ifndef _WaitMachine_h
#define _WaitMachine_h

#include "../Event.h"
#include "../StateMachine.h"

namespace kc1fsz {

class WaitMachine : public StateMachine {
public:

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setTargetTimeMs(uint32_t targetTime);

private:

    enum State { IDLE, OPEN } _state;
    uint32_t _targetTime;
};

}

#endif