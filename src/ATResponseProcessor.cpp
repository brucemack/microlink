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

/**
 * A function that is helpful when dealing with AT+ command protocols.
 * Locates either the token specified or \r\nERROR\r\n and returns its 
 * starting position in the accumulator provided.
 *
 * @param acc
 * @param accLen
 * @param loc This is where the location of the start of the 
 *   token is located.
 * @returns true if something was found, or false if nothing was found.
 *  
 */
bool findCompletionToken(const uint8_t* acc, uint32_t accLen, const char* token, 
    uint32_t* loc, uint32_t* len) {

    // Check for the target token
    const unsigned int tokenLen = strlen(token);

    for (unsigned int i = 0; i < accLen; i++) {

        unsigned int matchLen = 0;

        for (unsigned int k = 0; k < tokenLen && i + k < accLen; k++) {
            if (acc[i + k] == token[k]) {
                matchLen++;
            } else {
                break;
            }
        }
        // Did we match an entire term?
        if (matchLen == tokenLen) {
            *loc = i;
            *len = tokenLen;
            return true;
        }

        matchLen = 0;
        for (unsigned int k = 0; k < ERROR_TOKEN_LEN && i + k < accLen; k++) {
            if (acc[i + k] == ERROR_TOKEN[k]) {
                matchLen++;
            } else {
                break;
            }
        }
        // Did we match an entire term?
        if (matchLen == ERROR_TOKEN_LEN) {
            *loc = i;
            *len = ERROR_TOKEN_LEN;
            return true;
        }
    }

    return false;
}

static const uint32_t accSize = 256;
static uint8_t acc[accSize];
static uint32_t accLen = 0;

/**
 * A utilty function that is helpful when dealing with AT-style 
 * protcols.  Reads continuously from the channel looking for 
 * the completion token.  But can also preserve/return any 
 * "other" traffic that comes on the line (i.e. notifications).
 *
 * @param preText A pointer to a buffer that will be filled 
 *  with any "pre text" (i.e. unrelated things that show up before
 *  the completion token.
 * @returns true on success, false on ERROR
 */
bool waitOnCompletion(AsyncChannel& channel, const char* token, uint32_t timeOut,
    uint8_t* preText, uint32_t preTextSize, uint32_t* preTextLen) {

    while (true) {

        // Since we're in a blocking loop here, prompt the channel to 
        // make sure we're making forward progress in all the right ways.        
        channel.poll();

        if (channel.isReadable()) {
            
            // Read directly into the end of the accumulator
            uint32_t accFree = accSize - accLen;
            accLen += channel.read(acc + accLen, accFree);

            // Check for termination
            uint32_t tokenLoc = 0;
            uint32_t tokenLen = 0;
            bool b = findCompletionToken(acc, accLen, token, &tokenLoc, &tokenLen);
            if (b) {

                prettyHexDump(acc, accLen, cout);

                // Copy the pre-text (if any)
                if (tokenLoc > 0) {
                    for (unsigned int i = 0; 
                        i < preTextSize && i < tokenLoc; i++) {
                            preText[i] = acc[i];
                    }
                    *preTextLen = tokenLoc;
                }
                // Failure is when the ERROR token is found
                bool ret = acc[tokenLoc + 2] != 'E';
                // Flush the accumulator
                accLen = 0;
                return ret;
            }
        }
    }
    return false;
}

// Here we define the set of matchers that are used.  The lambda
// expression defines the "next move," assuming a match is
// match successfully.
ATResponseProcessor::Matcher ATResponseProcessor::_matchers[] = { 
    { MatchType::OK, false, "\r\nOK\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->ok();
            p._reset();
        }
    }, 
    { MatchType::SEND_OK, false, "\r\nSEND OK\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->sendOk();
            p._reset();
        }
    } ,
    { MatchType::ERROR, false, "\r\nERROR\r\n",
        [](ATResponseProcessor& p, const ATResponseProcessor::Matcher& m) { 
            p._sink->error();
            p._reset();
        }
    } ,
    { MatchType::SEND_PROMPT, false, "\r\n>",
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
    { MatchType::IPD, false, "\r\n+CIPDOMAIN:\"",
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
            p._sink->notification(p._acc, p._accUsed - 2);
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
    if (type == MatchType::OK ||
        type == MatchType::SEND_OK ||
        type == MatchType::ERROR ||
        type == MatchType::SEND_PROMPT ||
        type == MatchType::RECV_SIZE ||
        type == MatchType::DOMAIN ||
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
        // Just looking for a block of text that ends with 0x0d x00a
        if (lastByte == 0x0d && b == 0x0a) {
            return true;
        }
    }

    return false;
}

static int trace2 = 0;

ATResponseProcessor::ATResponseProcessor(EventSink* sink)
:   _sink(sink),
    _state(State::MATCHING) {
    _reset();
}

void ATResponseProcessor::process(const uint8_t* data, uint32_t dataLen) {
    for (uint32_t i = 0; i < dataLen; i++)
        _processByte(data[i]);
}

void ATResponseProcessor::_processByte(uint8_t b) {

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
        if (b == ':') {
            // Length finished, now collect the content. We 
            // reset the accumulator because we don't care about
            // the intro
            _accUsed = 0;
            _state = IN_IPD_2;
        } else if (isdigit(b)) {
            // Left shift one decial digit
            _ipdTotal *= 10;
            _ipdTotal += ((int)b - 0x30);
        }
    } else if (_state == State::IN_IPD_2) {    
        // Keep track of how much data we have received
        _ipdRecd++;
        // Received everything we expected?  If so, report out the 
        // final chunk and go into the state used to ignore the 
        // trailing 0x0d 0x0a (which are not part of the length)
        if (_ipdRecd == _ipdTotal) {
            _sink->ipd(_ipdChannel, _ipdChunks, _acc, _accUsed);
            _accUsed = 0;
            _state = IN_IPD_3;
        }
        // Accumluator full?  If so, hand off the latest chunk and 
        // keep going in the same state
        else if (_accUsed == _accSize) {
            _sink->ipd(_ipdChannel, _ipdChunks, _acc, _accUsed);
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
