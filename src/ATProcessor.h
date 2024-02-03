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
#ifndef _ATProcessor_h
#define _ATProcessor_h

#include <stdio.h>
#include <iostream>
#include <cctype>
#include <cstring>
#include <string>
#include <functional>

namespace kc1fsz {

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
bool findCompletionToken(const uint8_t* acc, uint32_t accLen, 
    const char* token, uint32_t* loc, uint32_t* len);

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
    uint8_t* preText, uint32_t preTextSize, uint32_t* preTextLen);

class ATProcessor {
public:

    /**
     * This interface is how the proessor reports its results to the 
     * outside world.
     */
    class EventSink {
    public:
        virtual void ok() = 0;
        virtual void sendOk() = 0;
        virtual void error() = 0;
        virtual void sendPrompt() = 0;
        virtual void sendSize() = 0;
        virtual void ipd(uint32_t channel, 
            const uint8_t* data, uint32_t len) = 0;
        virtual void closed(uint32_t channel) = 0;
        virtual void notification(const uint8_t* data, uint32_t len) = 0;
        virtual void confused() = 0;
    };

    ATProcessor(EventSink*sink);

    void process(const uint8_t* data, uint32_t dataLen);

private:

    // This is the function that does the heavy lifting
    void _processByte(uint8_t b);
    // Called to get back into the initial state
    void _reset();

    enum State {
        // Trying to match on something
        MATCHING,
        // Got the "Recv ", discarding the rest
        IN_RECV,
        // Got the "+IPD," processing the channel #
        IN_IPD_0,
        // Got the channel number and second comma, processing the length
        IN_IPD_1,
        // Got length and colon, processing the content
        IN_IPD_2,
        // Got all content, waiting past the final \r\n
        IN_IPD_3,
        // This state is used when the parse stream breaks and we
        // need to preserve state for debug
        HALTED
    };

    enum MatchType {
        OK,
        ERROR,
        SEND_OK,
        SEND_PROMPT,
        RECV_SIZE,
        IPD,
        CLOSED,
        NOTIFICATION
    };

    struct Matcher {

        MatchType type;
        bool alive;
        const char* target;
        std::function<void(ATProcessor&,const ATProcessor::Matcher&)> onSuccess;
        uint32_t matchPtr;
        uint32_t param;

        /**
         * @return true if a successful match has been found.
         */
        bool process(uint8_t lastByte, uint8_t b);

        void reset();
    };

    EventSink* _sink;
    State _state;
    uint32_t _ipdChannel;
    // A genral-purpose value that is used in a few places
    // where we need to remember numbers during the parse.
    uint32_t _ipdTotal;
    uint32_t _ipdRecd;

    // Here is where we accumulate data looking for a match.
    static const int _accSize = 64;
    uint8_t _acc[_accSize];
    uint32_t _accUsed;
    uint8_t _lastByte;

    static Matcher _matchers[]; 
};

}

#endif
