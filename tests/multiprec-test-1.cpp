#include <iostream>
#include <cstring>
#include <cstdint>
#include <cassert>

using namespace std;

/**
 * A multi-precision integer, unsigned.
 */
class mpx_u {
public:

    mpx_u(uint32_t* digits, unsigned maxDigits) : 
        _overflow(false),
        _digits(digits), 
        _maxDigits(maxDigits) {
        set(0);
    }

    bool isOf() const { return _overflow; }

    bool eq(const mpx_u& b) const {
        // Overflow makes comparison invalid
        if (_overflow || b._overflow)
            return false;
        unsigned k = max(_maxDigits, b._maxDigits);
        // Look for a disqualification
        for (unsigned i = 0; i < k; i++) {
            if (i >= _maxDigits) {
                if (b._digits[i] != 0)
                    return false;
            } else if (i >= b._maxDigits) {
                if (_digits[i] != 0)
                    return false;
            }
            else {
                if (b._digits[i] != _digits[i])
                    return false;
            }
        }
        return true;
    }

    bool gt(const mpx_u& b) const {
        // Overflow makes comparison invalid
        if (_overflow || b._overflow)
            return false;
        unsigned k = max(_maxDigits, b._maxDigits);
        // Start with the MSD and move backwards.
        for (unsigned i = 0; i < k; i++) {
            unsigned digit = k - i - 1;
            if (digit >= _maxDigits) {
                if (b._digits[i] > 0) {
                    return true;
                }
            } else if (digit >= b._maxDigits) {
                if (_digits[i] > 0)
                    return false;
            }
            else {
                if (b._digits[i] > _digits[i])
                    return true;
                else if (b._digits[i] < _digits[i])
                    return false;
            }
        }
        return false;
    }


    void set(const mpx_u& other) {
        set(0);
        for (unsigned i = 0; i < other._maxDigits; i++)
            setDigit(i, other._digits[i]);
    }

    void set(uint32_t a) {
        _overflow = false;
        memset(_digits, 0, _maxDigits * sizeof(uint32_t));
        setDigit(0, a);
    }

    void set(uint32_t a1, uint32_t a0) {
        _overflow = false;
        memset(_digits, 0, _maxDigits * sizeof(uint32_t));
        setDigit(1, a1);
        setDigit(0, a0);
    }

    void setDigit(unsigned place, uint32_t v) {
        assert(place < _maxDigits);
        _digits[place] = v;
    }

    uint32_t getDigit(unsigned place) const {
        assert(place < _maxDigits);
        return _digits[place];
    }

    /**
     * Adds the specified amount to a digit and propagate the 
     * carry to higher-order numbers if needed.
     */
    void addToDigit(unsigned place, uint32_t b) {
        if (b != 0) {
            if (place < _maxDigits) {
                // Do the add with extra space for the carry
                uint64_t a = (uint64_t)_digits[place] + (uint64_t)b;
                setDigit(place, a & 0xffffffff);
                uint64_t carry = a >> 32;
                if (carry != 0) {
                    if (place < _maxDigits - 1)
                        //addToDigit(place + 1, (carry & 0xffffffff));
                        addToDigit(place + 1, 1);
                    else 
                        _overflow = true;
                }
            }
            else {
                _overflow = true;
            }
        }
    }

    void add(const mpx_u& b) {
        for (unsigned i = 0; i < b._maxDigits; i++)
            addToDigit(i, b.getDigit(i));
    }

    /**
     * Subtracts and borrows if necessary.
     */
    void subtractFromDigit(unsigned place, uint32_t b) {
        if (b != 0) {
            if (place < _maxDigits) {
                // Do we have enough to subtract?
                if (_digits[place] >= b)
                    _digits[place] -= b;
                // Not enough in this position? Here is where we need to borrow 
                // from more significant digits.
                else {
                    subtractFromDigit(place + 1, 1);
                    // We actually get one more than this.
                    uint32_t v = 0xffffffff;
                    // Tweak
                    _digits[place] = (v - b) + 1;
                }
            } else {
                _overflow = true;
            }
        }
    }

    void subtract()

    void dumpHex(ostream& str) const {
        bool first = true;
        if (_overflow)
            cout << "(OF) ";
        bool leading = true;
        for (unsigned int d = 0; d < _maxDigits; d++) {
            unsigned int digit = _maxDigits - d - 1;
            if (digit > 0 && leading) 
                if (_digits[digit] == 0) 
                    continue;
            char temp[32];
            sprintf(temp,"%08X", _digits[digit]);
            if (!first)
                str << ",";
            str << temp;
            first = false;
        }
    }

    static void mult(const mpx_u& a, const mpx_u& b, mpx_u& result) {
        result.set(0);
        for (unsigned i = 0; i < a._maxDigits; i++) {
            for (unsigned j = 0; j < b._maxDigits; j++) {
                // Do this with extra space because of the potential carry
                uint64_t p = (uint64_t)a.getDigit(i) * (uint64_t)b.getDigit(j);
                // Increment current place
                result.addToDigit(i + j, (p & 0xffffffff));
                // Allow for carry into next place
                result.addToDigit(i + j + 1, p >> 32);
            }
        }
    }

 private:

    bool _overflow = false;
    uint32_t* _digits;
    const unsigned int _maxDigits;
};

int main(int,const char**) {
    
    uint32_t aw[2], bw[2], rw[2], cw[2], qw[4];
    mpx_u a(aw, 2), b(bw, 2), r(rw, 2), c(cw, 2), q(qw, 4);

    // Equality
    a.set(1);
    b.set(1);
    assert(a.eq(b));

    q.set(1);
    assert(a.eq(q));

    // GT
    a.set(1, 0);
    b.set(1);
    assert(a.gt(b));
    assert(!b.gt(a));
    b.set(1, 0);
    assert(!b.gt(a));

    // GT with different maxlen
    a.set(1,0);
    q.set(1);
    assert(!q.gt(a));

    // Simple carry test on add
    a.set(1);
    a.addToDigit(0,0xffffffff);
    assert(a.getDigit(0) == 0 && a.getDigit(1) == 1 && !a.isOf());

    // Simple carry test
    a.set(0xffffffff);
    a.addToDigit(0,0xffffffff);
    assert(a.getDigit(0) == 0xfffffffe && a.getDigit(1) == 1 && !a.isOf());

    // Multiplication carry test
    a.set(0xffffffff);
    b.set(0xffffffff);
    mpx_u::mult(a, b, r);
    cout << "0xffffffff x 0xffffffff=";
    r.dumpHex(cout);
    cout << endl;

    // Carry test during multiplication
    b.set(0, 0xffffffff);
    a.set(0, 2);
    mpx_u::mult(a, b, r);
    assert(r.getDigit(0) == 0xfffffffe && r.getDigit(1) == 1 && !r.isOf());

    // Force an overflow during multiplication
    a.set(0xffffffff, 0xffffffff);
    b.set(2);
    mpx_u::mult(a, b, r);
    assert(r.isOf());

    // Test of adding, showing carry
    a.set(0xffffffff);
    b.set(1);
    b.add(a);
    assert(b.getDigit(0) == 0 && b.getDigit(1) == 1 && !b.isOf());

    // Test of subtraction, showing borrow

}