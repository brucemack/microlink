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
#ifndef _ATResponseProcessor_h
#define _ATResponseProcessor_h

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

/**
 * A parser/state machine for parsing the response that comes back
 * from an ESP32 in AT comand mode.
 * 
 * IMPORTANT: This has nothing to do with constructing the requests
 * that are sent.
 *
 * It is assumed that echo has been disabled so that we don't have
 * to parse any characters related to requests.
*/
class ATResponseProcessor {
public:

    /**
     * This interface is how the proessor reports its results to the 
     * outside world.
     */
    class EventSink {
    public:
        virtual void ok() { }
        virtual void sendOk()  { }
        virtual void error()  { }
        virtual void ready()  { }
        virtual void sendPrompt() { }
        virtual void sendSize()  { }
        virtual void domain(const char* addr)  { }
        virtual void ipd(uint32_t channel, uint32_t chunk,
            const uint8_t* data, uint32_t len)  { }
        virtual void connected(uint32_t channel)  { }
        virtual void closed(uint32_t channel)  { }
        virtual void notification(const char* msg);
        virtual void confused(const uint8_t* data, uint32_t len)  { }
    };

    ATResponseProcessor(EventSink* sink);

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
        // Got the "+CIPDOMAIN:" and processing the content
        IN_DOMAIN_0,
        // Got the address and waiting past the final \r\n
        IN_DOMAIN_1,
        // This state is used when the parse stream breaks and we
        // need to preserve state for debug
        HALTED,
        IDLE
    };

    enum MatchType {
        OK,
        ERROR,
        READY,
        SEND_OK,
        SEND_PROMPT,
        RECV_SIZE,
        IPD,
        DOMAIN,
        CONNECT,
        CLOSED,
        NOTIFICATION
    };

    struct Matcher {

        MatchType type;
        bool alive;
        const char* target;
        std::function<void(ATResponseProcessor&,const ATResponseProcessor::Matcher&)> onSuccess;
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
    // The total size of the IPD data as reported by the +IPD message
    uint32_t _ipdTotal;
    // How much of the IPD data has been received so far
    uint32_t _ipdRecd;
    // How many "chunks" the IPD data have been transfered
    uint32_t _ipdChunks;

    // Here is where we accumulate data looking for a match.
    static const int _accSize = 256;
    uint8_t _acc[_accSize];
    uint32_t _accUsed;
    uint8_t _lastByte;

    static Matcher _matchers[]; 
};

}

#endif
