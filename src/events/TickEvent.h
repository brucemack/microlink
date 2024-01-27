#ifndef _TickEvent_h
#define _TickEvent_h

#include <cstdint>

#include "../common.h"
#include "../Event.h"

namespace kc1fsz {

class TickEvent : public Event {
public:

    static const int TYPE = 104;

    TickEvent() : Event(TYPE) { }
};

}

#endif

