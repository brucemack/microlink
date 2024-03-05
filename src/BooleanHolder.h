#ifndef _BooleanHolder_h
#define _BooleanHolder_h

namespace kc1fsz {

class BooleanHolder {
public:
    BooleanHolder(bool* b) : _b(b) { *_b = true; }
    ~BooleanHolder() { *_b = false; }
private:
    bool* _b = 0;
};

}

#endif
