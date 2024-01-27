#ifndef _WaitMachine_h
#define _WaitMachine_h

#include "../Event.h"
#include "../Context.h"
#include "../StateMachine.h"

namespace kc1fsz {

class WaitMachine : public StateMachine<Context> {
public:

    virtual void processEvent(const Event* ev, Context* ctx);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setTargetTimeMs(uint32_t targetTime);

private:

    enum State { IDLE, OPEN } _state;
    uint32_t _targetTime;
};

}

#endif