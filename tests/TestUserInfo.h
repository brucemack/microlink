#ifndef _TestUserInfo_h
#define _TestUserInfo_h

#include <iostream>
#include "../src/UserInfo.h"

namespace kc1fsz {

class TestUserInfo : public UserInfo {
public:

    virtual void setStatus(const char* msg) { std::cout << "UserInfo(Status): " << msg << std::endl; }
    virtual void setOnData(const char* msg) { std::cout << "UserInfo(oNDATA): [" << msg << "]" << std::endl; }
};

}

#endif
