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
 */
#ifndef _SIM7600IPLib_h
#define _SIM7600IPLib_h

#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/IPLib.h"
#include "kc1fsz-tools/CircularQueuePtr.h"

namespace kc1fsz {

class Log;
class AsyncChannel;

/**
 * Used to track a send waiting to be sent to the tunnel
 */
struct QueuedSend {

    QueuedSend() : dataLen(0) { }

    QueuedSend(const uint8_t* d, uint32_t dl) {
        memcpyLimited(data, d, dl, 256);
        dataLen = dl;
    }

    uint8_t data[256];
    uint32_t dataLen;
};

/**
 * IMPORTANT: We are assuming that this runs on an embedded processor
 * we so limit the use of C++ features.
 */
class SIM7600IPLib : public IPLib, public Runnable {
public:

    static int traceLevel;

    SIM7600IPLib(Log* log, AsyncChannel* uart, uint32_t resetPin);

    // ----- Runnable Methods ------------------------------------------------

    /**
     * This should be called from the event loop.  It attempts to make forward
     * progress and passes all events to the event processor.
    */
    virtual void run();

    // ----- From IPLib ------------------------------------------------------

    virtual void reset();

    virtual bool isLinkUp() const;

    virtual void addEventSink(IPLibEvents* e);

    virtual void queryDNS(HostName hostName);

    virtual Channel createTCPChannel();
    virtual void connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port);
    virtual void sendTCPChannel(Channel c, const uint8_t* b, uint16_t len);

    virtual Channel createUDPChannel();
    virtual void bindUDPChannel(Channel c, uint32_t port);
    virtual void sendUDPChannel(const Channel& c, 
        const IPAddress& remoteIpAddr, uint32_t remotePort,
        const uint8_t* b, uint16_t len);

    virtual void closeChannel(Channel c);

private:

    void _write(const uint8_t* data, uint32_t dataLen);
    void _write(const char* cmd);

    /**
     * Queues a packet to be sent to the tunnel.  This is 
     * paritcularly necessary since we won't be allowed to 
     * send commands when in the middle of a previous AT+CIPSEND sequence
     * that involves some modal prompts.
     */
    void _queueSend(const uint8_t* data, uint32_t dataLen);

    void _processLine(const char* data, uint32_t dataLen);
    void _processIPD(const uint8_t* data, uint32_t dataLen);
    void _processProxyFrameIfPossible();
    void _processProxyFrame(const uint8_t* data, uint32_t dataLen);

    Log* _log;
    AsyncChannel* _uart;
    uint32_t _resetPin;

    static const uint32_t _maxEvents = 16;
    IPLibEvents* _events[_maxEvents];
    uint32_t _eventsLen = 0;   

    static const uint32_t _sendQueueSize = 4;
    QueuedSend _sendQueue[_sendQueueSize];
    CircularQueuePtr _sendQueuePtr;
    QueuedSend _workingSend;

    static const uint32_t _rxHoldSize = 4096;
    uint8_t _rxHold[_rxHoldSize];
    uint32_t _rxHoldLen = 0;

    static const uint32_t _ipdHoldSize = 2048;
    uint8_t _ipdHold[_ipdHoldSize];
    uint32_t _ipdHoldLen = 0;

    enum State {
        IDLE,
        INIT_0a,
        INIT_0b,
        INIT_0c,
        INIT_0,
        INIT_1,
        INIT_2,
        INIT_3,
        INIT_3a,
        INIT_4,
        INIT_5,
        INIT_5a,
        INIT_CSQ_0,
        INIT_CSQ_1,
        // Pause before OPEN
        INIT_6h,
        // Ready to send OPEN to tunnel
        INIT_6,
        // Waiting for OK after CIOPEN 
        INIT_7,
        // (17) Waiting for result of CIOPEN
        INIT_7a,
        // (18) Reconnect - ready to send close
        RECON_0,
        // (19) Wiating for OK on cose
        RECON_1,
        // (20)
        RUN,
        // (21) Waiting for the > prompt on the +CIPSEND
        SEND_1,
        // (22) Waiting for the final OK on the +CIPSEND
        SEND_2,
        SEND_3,
        FAILED
    };

    State _state = State::IDLE;
    bool _isNetOpen = false;
    bool _inIpd = false;
    uint32_t _ipdLen = 0;

    int _channelCount = 1;
    IPAddress _lastAddr;
    uint16_t _lastPort;
    timestamp _stateTime;
};

}

#endif
