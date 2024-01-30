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
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include <cassert>
#include <iostream>
#include <algorithm>

#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/events/TCPDisconnectEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/Common.h"

#include "../common.h"
#include "../EventProcessor.h"

#include "SocketContext.h"

using namespace  std;

namespace kc1fsz {

SocketContext::SocketContext()
:   _dnsResultPending(false) {
}

bool SocketContext::poll(EventProcessor* ep) {

    bool anythingHappened = false;

    // TODO: NICE QUEUE NEEDED HERE
    // Check for any pending DNS results (making a synchronous event look
    // asynchronous).
    if (_dnsResultPending) {
        _dnsResultPending = false;
        ep->processEvent(&_dnsResult);
        anythingHappened = true;
    }

    if (!_tracker.empty()) {
       
        fd_set readfds, writefds; 
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        int highestFd = 0;

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        auto setupSelect = [&readfds, &writefds, &highestFd](const SocketTracker& t) { 
            if (t.deletePending) {
                return;
            }
            if (t.type == SocketTracker::Type::TCP) {
                if (t.connectRequested && t.connectWaiting) {
                    FD_SET(t.fd, &writefds);
                } else if (t.connectRequested) {
                    FD_SET(t.fd, &readfds);
                }
            } else if (t.type == SocketTracker::Type::UDP) {
                FD_SET(t.fd, &readfds);
            }
            if (t.fd > highestFd) {
                highestFd = t.fd;
            }
        }; 

        // A lambda that is used to process the results of the call to select()
        auto processSelect = [&readfds, &writefds, &ep, &anythingHappened](SocketTracker& t) { 
            if (t.deletePending) {
                return;
            }

            // The handling of the socket events depends on the type of socket

            if (t.type == SocketTracker::Type::TCP) {
                // If the channel is waiting for a connect and we see write ready
                // then notify that the connection is a success.
                if (t.connectWaiting) {
                    if (FD_ISSET(t.fd, &writefds)) {
                        // No longer waiting on the connection - now normal
                        t.connectWaiting = false;
                        // Generate an event
                        TCPConnectEvent ev(Channel(t.fd));
                        ep->processEvent(&ev);
                        anythingHappened = true;
                    } 
                }
                // If the channel is connected and we see that read ready then 
                // pull some data off the socket and generate a data event.
                else {
                    if (FD_ISSET(t.fd, &readfds)) {
                        char buffer[256];
                        int rc = recv(t.fd, buffer, 256, 0);
                        // Got any bytes?
                        if (rc > 0) {
                            // Generate an event
                            TCPReceiveEvent ev(Channel(t.fd), (const uint8_t*)buffer, rc);
                            ep->processEvent(&ev);
                            anythingHappened = true;
                        } 
                        // Check if the other side has dropped?
                        else if (rc == 0) {
                            // Ask for cleanup on next pass
                            t.deletePending = true;
                            // Generate an event
                            TCPDisconnectEvent ev(Channel(t.fd));
                            ep->processEvent(&ev);
                            anythingHappened = true;
                        }
                    }
                }
            }
            else if ( t.type == SocketTracker::Type::UDP) {
                // For UDP it's just about receiving data
                if (FD_ISSET(t.fd, &readfds)) {
                    char buffer[256];
                    // TODO: INCLUDE SOURCE ADDRESS
                    int rc = recv(t.fd, buffer, 256, 0);
                    // Got any bytes?
                    if (rc > 0) {
                        //cout << "----- UDP Receive " << t.fd << endl;
                        //prettyHexDump((const uint8_t*)buffer, rc, cout);
                        // Generate an event
                        UDPReceiveEvent ev(Channel(t.fd), (const uint8_t*)buffer, rc);
                        ep->processEvent(&ev);
                        anythingHappened = true;
                    } 
                }
            }
        }; 

        _cleanupTracker();

        // Prepare for select by looking at each tracker entry 
        std::for_each(_tracker.begin(), _tracker.end(), setupSelect);

        // Do the real thing
        int rv = select(highestFd + 1, &readfds, &writefds, NULL, &tv); 
        if (rv > 0) {
            // React based on what select() detected
            std::for_each(_tracker.begin(), _tracker.end(), processSelect);
        }
    }

    return anythingHappened;
}

void SocketContext::_cleanupTracker() {

    auto cleanup = [](const SocketTracker& t) { 
        return t.deletePending;
    };

    // Purge any dead activity
    std::remove_if(_tracker.begin(), _tracker.end(), cleanup);
}

int SocketContext::getLiveChannelCount() const {
    // Effectively const
    ((SocketContext*)this)->_cleanupTracker();
    return _tracker.size();
}

void SocketContext::startDNSLookup(HostName hostName) {
    // TODO: UPGRADE FOR IP6
    const struct hostent* remoteHost = gethostbyname(hostName.c_str());
    if (remoteHost->h_addrtype == AF_INET && remoteHost->h_length == 4) {
        uint32_t addr_nl = *(uint32_t*)remoteHost->h_addr_list[0];
        _dnsResultPending = true;
        _dnsResult = DNSLookupEvent(hostName, addr_nl);
    }
}

Channel SocketContext::createTCPChannel() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return Channel(0, false);
    }
    // Make non-blocking
    fcntl(fd, F_SETFL, O_NONBLOCK); 
    // Setup the tracker
    SocketTracker t;
    t.type = SocketTracker::Type::TCP;
    t.fd = fd;
    _tracker.push_back(t);
    // The channel ID maps directly to the fd
    return Channel(fd);
}

void SocketContext::closeTCPChannel(Channel c) {
    _closeChannel(c);
}

void SocketContext::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {

    if (!c.isGood()) {
        return;
    }
    // Find the existing tracker for this channel
    auto it = std::find_if(_tracker.begin(), _tracker.end(), 
        [&c](const SocketTracker& arg) { return arg.fd == c.getId(); });
    if (it == _tracker.end()) {
        return;
    } 
    if (it->type != SocketTracker::Type::TCP) {
        return;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(sockaddr_in));
    remote_addr.sin_family = AF_INET;
    // Address is already in NL order
    remote_addr.sin_addr.s_addr = ipAddr.getAddr();
    remote_addr.sin_port = htons(port);

    // Launch a non-blocking connect    
    int rc = connect(c.getId(), (const sockaddr*)&remote_addr, sizeof(sockaddr_in));
    if (rc == 0) {
        // TODO: CONSIDER THIS CASE
        assert(false);
    }
    else if (rc == -1 && errno == EINPROGRESS) {
        it->connectRequested = true;
        it->connectWaiting = true;
    } else {
        cout << "Connect failed" << endl;
    }
}

void SocketContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {
    if (c.isGood()) {
        // TODO: IT'S POSSIBLE THAT THE SEND IS INCOMPLETE?
        int rc = send(c.getId(), (char *)b, len, 0);
        if(rc > 0) {
            cout << "Sent" << endl;
            prettyHexDump(b, len, cout);
        }
    }
}

Channel SocketContext::createUDPChannel(uint32_t localPort) {

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return Channel(0, false);
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(localPort);

    int rc = bind(fd, (struct sockaddr*)&local_addr, sizeof(sockaddr_in));
    if (rc < 0) {
        close(fd);
        return Channel(0, false);
    }

    // Sockets non-blocking
    fcntl(fd, F_SETFL, O_NONBLOCK); 

    // Setup the tracker
    SocketTracker t;
    t.type = SocketTracker::Type::UDP;
    t.fd = fd;
    _tracker.push_back(t);

    return Channel(fd);
}

void SocketContext::closeUDPChannel(Channel c) {  
    _closeChannel(c);
}

void SocketContext::_closeChannel(Channel c) {  
    // The channel ID maps directly to the fd
    close(c.getId());
    // Find/remove tracker
    auto it = std::find_if(_tracker.begin(), _tracker.end(), 
        [&c](const SocketTracker& arg) { return arg.fd == c.getId(); });
    if (it != _tracker.end()) {
        _tracker.erase(it);
    }
}

void SocketContext::sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
    const uint8_t* b, uint16_t len) {

    char buf[64];
    formatIP4Address(targetAddr.getAddr(), buf, 64);

    cout << "UDP Send to " << buf << ":" << targetPort << endl;
    prettyHexDump(b, len, cout);

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(sockaddr_in));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = targetAddr.getAddr();
    remote_addr.sin_port = htons(targetPort);

    int rc = sendto(c.getId(), b, len, 0, (const struct sockaddr*)&remote_addr, sizeof(sockaddr_in));
    if (rc <= 0) {
        cout << "SEND FAILED" << endl;
    }
}

}

