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
#include <stdio.h>
#include <iostream>
#include <cctype>
#include <cstring>
#include <string>

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/AsyncChannel.h"
#include "ATResponseProcessor.h"

using namespace std;

namespace kc1fsz {

static constexpr const char* ERROR_TOKEN = "\r\nERROR\r\n";
static constexpr int ERROR_TOKEN_LEN = std::char_traits<char>::length(ERROR_TOKEN);

// Here we define the set of matchers that are used.  The lambda
// expression defines the "next move," assuming a match is
// match successfully.
ATResponseProcessor::Matcher ATResponseProcessor::_matchers[] = { 
    { MatchType::SIMPLE, false, "\r\nOK\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->ok();
            p._reset();
        }
    }, 
    { MatchType::SIMPLE, false, "\r\nSEND OK\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->sendOk();
            p._reset();
        }
    } ,
    { MatchType::SIMPLE, false, "\r\nSEND FAIL\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->sendFail();
            p._reset();
        }
    } ,
    { MatchType::SIMPLE, false, "\r\nERROR\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->error();
            p._reset();
        }
    } ,
    { MatchType::SIMPLE, false, "\r\nready\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->ready();
            p._reset();
        }
    } ,
    { MatchType::SIMPLE, false, "\r\n>",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->sendPrompt();
            p._reset();
        }
    } ,
    { MatchType::RECV_SIZE, false, "\r\nRecv ",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._state = State::IN_RECV;
        }
    } ,
    { MatchType::DOMAIN_ADDR, false, "+CIPDOMAIN:\"",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            // Flush the accumulator
            p._accUsed = 0;
            p._state = State::IN_DOMAIN_0;
        }
    },
    { MatchType::IPD, false, "\r\n+IPD,",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._state = State::IN_IPD_0;
        }
    },
    { MatchType::CONNECT, false, "#,CONNECT\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->connected(m.param);
            p._reset();
        }
    } ,
    { MatchType::CLOSED, false, "#,CLOSED\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->closed(m.param);
            p._reset();
        }
    } ,
    { MatchType::NOTIFICATION, false, 0,
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            // Null terminate
            p._acc[p._accUsed - 2] = 0;
            p._sink->notification((const char*)p._acc);
            p._reset();
        }
    } 
};

void ATResponseProcessor::Matcher::reset() {
    alive = true;
    matchPtr = 0;
}

bool ATResponseProcessor::Matcher::process(uint8_t lastByte, uint8_t b) {
    // The processing/matching process depends on the type being used
    if (type == MatchType::SIMPLE ||
        type == MatchType::RECV_SIZE ||
        type == MatchType::DOMAIN_ADDR ||
        type == MatchType::IPD) {
        // Still going?
        if ((char)(target[matchPtr]) == b) {
            matchPtr++;
            // Complete match?
            if (target[matchPtr] == 0) {
                return true;
            }
        } 
        // If any of the individual matches fails then this is dead.
        else {
            alive = false;
        }
    }
    else if (type == MatchType::CLOSED ||
             type == MatchType::CONNECT) {
        // Still going?
        if ((char)(target[matchPtr]) == b ||
            ((char)(target[matchPtr]) == '#' && isdigit(b))) {
            // Capture the number if necessary
            if ((char)(target[matchPtr]) == '#') {
                param = b - (uint8_t)0x30;
            }
            matchPtr++;
            // Complete match?
            if (target[matchPtr] == 0) {
                return true;
            }
        }
        // If any of the individual matches fails then this is dead.
        else {
            alive = false;
        }
    }
    else if (type == MatchType::NOTIFICATION) {
        // NOtifications need to start with a letter
        if (matchPtr == 0 && !isalpha(b)) {
            alive = false;
        }
        matchPtr++;
        // Just looking for a block of text that ends with 0x0d x0a
        if (lastByte == 0x0d && b == 0x0a) {
            return true;
        }
    }

    return false;
}

ATResponseProcessor::ATResponseProcessor(EventSink* sink)
:   _sink(sink),
    _state(State::IDLE) {
    _reset();
}

void ATResponseProcessor::process(const uint8_t* data, uint32_t dataLen) {
    for (uint32_t i = 0; i < dataLen; i++)
        _processByte(data[i]);
}

void ATResponseProcessor::_processByte(uint8_t b) {

    if (_state == State::IDLE) {
        _state = State::MATCHING;
    }

    if (_state == State::HALTED) {
        return;
    }

    // Make sure we never overflow the accumulator
    if (_accUsed == _accSize) {
        _sink->confused(_acc, _accUsed);
        _reset();
        return;
    }

    // Accumulate the byte in case it's needed
    _acc[_accUsed++] = b;

    // The parsing depends on what state we are in.  There are some 
    // special cases when receive data asyncronously.

    if (_state == State::MATCHING) {    

        int liveMatchers = 0;

        // Go trough the remaining live matchers and try to get a hit 
        // on something
        for (Matcher& m : _matchers) {
            // Only work on the live matchers
            if (m.alive) {
                liveMatchers++;
                if (m.process(_lastByte, b)) {
                    m.onSuccess(*this, m);
                    return;                        
                }
            }
        }

        // This is an error state - no matchers have anything.
        if (liveMatchers == 0) {
            _sink->confused(_acc, _accUsed);
            _state = State::HALTED;
            return;
        }
    } 
    // Here we have +CIPDOMAIN:" and are getting the IP address
    else if (_state == State::IN_DOMAIN_0) {    
        // Closing quote ends things
        if (b == '\"') {
            // We don't send the closing quote
            if (_accUsed > 0)
                _acc[_accUsed - 1] = 0;
            _sink->domain((const char*)_acc);
            _state = State::IN_DOMAIN_1;
        }
    }
    // This state is used to wait for the final 0x0a after the end of 
    // the content.
    else if (_state == State::IN_DOMAIN_1) {    
        if (b == 0x0a) {
            _reset();
        }
    }    
    // Here we have +IPD, and are getting the channel
    else if (_state == State::IN_IPD_0) {    
        if (b == ',') {
            // Channel finished, now collect the length
            _state = IN_IPD_1;
        } else if (b == ' ') {
            // NOTHING (ignore spaces)
        } else if (isdigit(b)) {
            // Left shift one decial digit
            _ipdChannel *= 10;
            _ipdChannel += ((int)b - 0x30);
        }
    }
    // Here we have +IPD,ccc, and are getting the length
    else if (_state == State::IN_IPD_1) {    
        if (b == ',') {
            // Length finished, now collect the content. We 
            // reset the accumulator because we don't care about
            // the intro
            _accUsed = 0;
            _state = IN_IPD_1a;
        } else if (isdigit(b)) {
            // Left shift one decial digit
            _ipdTotal *= 10;
            _ipdTotal += ((int)b - 0x30);
        }
    }
    // Here we have +IPD,ccc,llll, and are ready to ignore the opening 
    // quote for the addres  
    else if (_state == State::IN_IPD_1a) {  
        _accUsed = 0;
        _state = State::IN_IPD_1b;  
    }
    // Here we have +IPD,ccc,llll," and are accumulating the source
    // address until the closing quote.
    else if (_state == State::IN_IPD_1b) {
        if (b == '"') {
            // We don't include the closting quote
            if (_accUsed > 0)
                _acc[_accUsed - 1] = 0;
            strcpyLimited(_ipdAddr, (const char*)_acc, _ipdAddrSize);
            _state = State::IN_IPD_1c;  
        }
    }
    // Here we have +IPD,ccc,llll,"x.x.x.x" and are ignoring the port
    // number.
    else if (_state == State::IN_IPD_1c) {
        if (b == ':') {
            _state = State::IN_IPD_2;  
            _accUsed = 0;
        }
    }
    else if (_state == State::IN_IPD_2) {    
        // Keep track of how much data we have received
        _ipdRecd++;
        // Received everything we expected?  If so, report out the 
        // final chunk and go into the state used to ignore the 
        // trailing 0x0d 0x0a (which are not part of the length)
        if (_ipdRecd == _ipdTotal) {
            _sink->ipd(_ipdChannel, _ipdChunks, _acc, _accUsed, _ipdAddr);
            _accUsed = 0;
            _state = IN_IPD_3;
        }
        // Accumluator full?  If so, hand off the latest chunk and 
        // keep going in the same state
        else if (_accUsed == _accSize) {
            _sink->ipd(_ipdChannel, _ipdChunks, _acc, _accUsed, _ipdAddr);
            _accUsed = 0;
            _ipdChunks++;
        }
    }
    // This state is used to wait for the final 0x0a after the end of 
    // the content.
    else if (_state == State::IN_IPD_3) {    
        if (b == 0x0a) {
            _reset();
        }
    }
    // Here we have "Recv " and are discarding the rest
    else if (_state == State::IN_RECV) {    
        if (b == 0x0a) {
            _sink->sendSize();
            _reset();
        } 
    }

    _lastByte = b;
}

void ATResponseProcessor::_reset() {

    _state = MATCHING;
    _ipdChannel = 0;
    _ipdTotal = 0;
    _ipdChunks = 0;
    _ipdRecd = 0;
    _accUsed = 0;
    _lastByte = 0;

    for (Matcher& m : _matchers)
        m.reset();
}

}
