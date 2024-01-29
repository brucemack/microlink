#include "WaitMachine.h"

namespace kc1fsz {

void WaitMachine::processEvent(const Event* ev) {

}

void WaitMachine::start() {

}

bool WaitMachine::isDone() const {
    return false;
}

bool WaitMachine::isGood() const {
    return false;
}

void WaitMachine::setTargetTimeMs(uint32_t targetTime) {
    _targetTime = targetTime;
}

}

