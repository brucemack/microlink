/**
 * MicroLink EchoLink Station
 * Copyright (C) 2024, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * FOR AMATEUR RADIO USE ONLY.
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 * 
 * =================================================================================
 * This file is unit-test code only.  None of this should be use for 
 * real applications!
 * =================================================================================
 */
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <string>

#include "contexts/SocketContext.h"
#include "machines/RootMachine.h"
#include "TestUserInfo.h"

using namespace std;
using namespace kc1fsz;

int main(int, const char**) {

    SocketContext context;

    TestUserInfo info;
    RootMachine rm(&info);
    rm.setServerName(HostName("naeast.echolink.org"));
    rm.setCallSign(CallSign("KC1FSZ"));
    rm.setPassword(FixedString("XYZ123"));
    rm.setLocation(FixedString("Wellesley, MA USA"));
    rm.setTargetCallSign(CallSign("W1TKZ-L"));

    // Here is the main event loop
    while (true) {
        // Tell the context to move forward
    }
}
