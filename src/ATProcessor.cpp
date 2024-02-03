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
#include "ATProcessor.h"

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

ATProcessor::ATProcessor(EventSink* sink)
:   _sink(sink),
    _state(State::MATCHING) {
    _reset();
}

void ATProcessor::process(const uint8_t* data, uint32_t dataLen) {
    for (uint32_t i = 0; i < dataLen; i++)
        _processByte(data[i]);
}

void ATProcessor::_processByte(uint8_t b) {

    if (_state == State::HALTED) {
        return;
    }

    // Make sure we never overflow the accumulator
    if (_accUsed == _accSize) {
        _sink->confused();
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
                // What we do depends on the matcher type
                if (m.type == MatchType::OK ||
                    m.type == MatchType::SEND_OK ||
                    m.type == MatchType::ERROR ||
                    m.type == MatchType::SEND_PROMPT ||
                    m.type == MatchType::RECV_SIZE ||
                    m.type == MatchType::IPD) {
                    // Still going?
                    if ((char)(m.target[_matchPtr]) == b) {
                        // Complete match?
                        if (m.target[_matchPtr + 1] == 0) {
                            if (m.type == MatchType::OK) {
                                _sink->ok();
                                _reset();
                                return;
                            } else if (m.type == MatchType::SEND_OK) {
                                _sink->sendOk();
                                _reset();
                                return;
                            } else if (m.type == MatchType::ERROR) {
                                _sink->error();
                                _reset();
                                return;
                            } else if (m.type == MatchType::SEND_PROMPT) {
                                _sink->sendPrompt();
                                _reset();
                                return;
                            } else if (m.type == MatchType::RECV_SIZE) {
                                _state = State::IN_RECV;
                            } else if (m.type == MatchType::IPD) {
                                _state = State::IN_IPD_0;
                            }
                        }
                    } 
                    // If any of the individual matches fails then this is dead.
                    else {
                        m.alive = false;
                    }
                }
                else if (m.type == MatchType::CLOSED) {
                    // Still going?
                    if ((char)(m.target[_matchPtr]) == b ||
                        ((char)(m.target[_matchPtr]) == '#' && isdigit(b))) {
                        // Capture the number if necessary
                        if ((char)(m.target[_matchPtr]) == '#') {
                            m.param = b - (uint8_t)0x30;
                        }
                        // Complete match?
                        if (m.target[_matchPtr + 1] == 0) {
                            _sink->closed(m.param);
                            _reset();
                            return;
                        }
                    }
                    // If any of the individual matches fails then this is dead.
                    else {
                        m.alive = false;
                    }
                }
                else if (m.type == MatchType::NOTIFICATION) {
                    // NOtifications need to start with a letter
                    if (_accUsed == 1 && !isalpha(b)) {
                        m.alive = false;
                    }
                    // Just looking for a block of text that ends with 0x0d x00a
                    else if (_lastByte == 0x0d && b == 0x0a) {
                        _sink->notification(_acc, _accUsed - 2);
                        _reset();
                        return;
                    }
                }
            }
        }
        // This is an error state - no matchers have anything.
        if (liveMatchers == 0) {
            _sink->confused();
            _state = State::HALTED;
            return;
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
        // final chunk and reset.
        if (_ipdRecd == _ipdTotal) {
            _sink->ipd(_ipdChannel, _acc, _accUsed);
            _reset();
            return;
        }
        // Accumluator full?  If so, hand off the latest chunk and 
        // keep going in the same state
        else if (_accUsed == _accSize) {
            _sink->ipd(_ipdChannel, _acc, _accUsed);
            _accUsed = 0;
        }
    }
    // Here we have "Recv " and are discarding the rest
    else if (_state == State::IN_RECV) {    
        if (b == 0x0a) {
            _sink->sendSize();
            _reset();
            return;
        } 
    }

    _lastByte = b;
    _matchPtr++;
}

void ATProcessor::_reset() {
    _state = MATCHING;
    _ipdChannel = 0;
    _ipdTotal = 0;
    _ipdRecd = 0;
    _accUsed = 0;
    _lastByte = 0;
    _matchPtr = 0;
    for (int i = 0; i < _matchersSize; i++)
        _matchers[i].alive = true;
}

}
