#ifndef _Event_h
#define _Event_h

namespace kc1fsz {

class Event {
public:

    Event(int type) : _type(type) { }

    int getType() const {
        return _type;
    }

protected:

    int _type;
};

}

#endif
