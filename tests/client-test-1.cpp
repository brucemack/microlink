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
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"
#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/wav_util.h"

using namespace std;
using namespace kc1fsz;

static uint32_t waitForClose(int sock, uint8_t* packet, uint32_t packetSize, uint32_t waitMs) {
    
    uint32_t used = 0;
    uint32_t startMs = time_ms();

    // Loop until timeout
    while (time_ms() - startMs < waitMs) {
        int rc = recv(sock, packet + used, packetSize - used, 0);
        if (rc > 0) {
            used += rc;
        } else {
            break;
        }
    }
    return used;
}

static int sendHello(int rtpSock, int rtcpSock, 
    const sockaddr_in& rtp_remote_addr,
    const sockaddr_in& rtcp_remote_addr, 
    const char* callSign,
    const char* fullName,
    uint32_t ssrc, uint32_t ssrc2,
    bool keepAlive) {

    int len, rc;
    const uint32_t packetSize = 256;
    uint8_t p[packetSize];

    len = formatRTCPPacket_SDES(ssrc, callSign, fullName, ssrc2, p, packetSize);
    cout << "RTCP SDES packet (length=" << len << "):" << endl;
    prettyHexDump(p, len, cout);

    if (!keepAlive) {
        // First send is on the RTP socket!
        rc = sendto(rtpSock, p, len, 0, (const struct sockaddr*)&rtcp_remote_addr, sizeof(sockaddr_in));
        if (rc < 0) {
            cout << "Send error 0" << endl;
            return -1;
        }
        usleep(10 * 1000);
    }

    // Second send is on the RTCP socket!
    rc = sendto(rtcpSock, p, len, 0, (const struct sockaddr*)&rtcp_remote_addr, sizeof(sockaddr_in));
    if (rc < 0) {
        cout << "Send error 1" << endl;
        return -1;
    }
    
    // Send the initial oNDATA message
    const char* msg = "oNDATA\rStation KC1FSZ\r\rMicroLink ver 0.02\r\rBruce R. MacKinnon\rWellesley, MA USA\r";
    len = formatOnDataPacket(msg, ssrc2, p, packetSize);
    cout << "RTP oNDATA packet (length=" << len << "):" << endl;
    prettyHexDump(p, len, cout);
    
    rc = sendto(rtpSock, p, len, 0, (const struct sockaddr*)&rtp_remote_addr, sizeof(sockaddr_in));
    if (rc < 0) {
        cout << "Send error 2" << endl;
        return -1;
    }

    return 0;
}

static int sendToDirectoryServer(const char* ipAddr, uint8_t* packet, uint32_t packetLen) {

    const int dirPort = 5200;

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(sockaddr_in));
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddr, &(remote_addr.sin_addr));
    remote_addr.sin_port = htons(dirPort);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cout << "Socket failed" << endl;
        return -1;
    }

    int rc = connect(sock, (const sockaddr*)&remote_addr, sizeof(sockaddr_in));
    if (rc < 0) {
        cout << "Connect failed" << endl;
        close(sock);
        return -1;
    }

    rc = send(sock, (char *)packet, packetLen, 0);
    if (rc != (int)packetLen) {
        cout << "Directory send failure" << endl;
        close(sock);
        return -1;
    }

    const uint32_t respPacketSize = 256;
    uint8_t respPacket[respPacketSize];
    uint32_t respBytesReceived = waitForClose(sock, respPacket, respPacketSize, 1000);
    prettyHexDump(respPacket, respBytesReceived, cout);

    close(sock);

    if (respBytesReceived > 2 && respPacket[0] == 'O' && respPacket[1] == 'K') {
        return 0;
    } else {
        return -1;
    }
}

static int sendOnline(const char* callSign, const char* password, const char* loc) {

    // Get the remote addresses setup
    const char* addr = "129.213.119.249";

    time_t t = time(0);
    struct tm tm;
    char local_time_str[6];
    strftime(local_time_str, 6, "%H:%M", localtime_r(&t, &tm));

    uint8_t message[256];
    uint8_t* p = message;

    (*p++) = 'l';
    memcpy(p, callSign, strlen(callSign));
    p += strlen(callSign);
    (*p++) = 0xac;
    (*p++) = 0xac;
    memcpy(p, password, strlen(password));
    p += strlen(password);
    (*p++) = 0x0d;
    memcpy(p, "ONLINE", 10);
    p += 6;
    // Version (MUST START WITH NUMBER AND SHOULD END WITH Z)
    memcpy(p, "0.02MLZ", 7);
    p += 7;
    (*p++) = '(';
    memcpy(p, local_time_str, 5);
    p += 5;
    (*p++) = ')';
    (*p++) = 0x0d;
    memcpy(p, loc, strlen(loc));
    p += strlen(loc);
    (*p++) = 0x0d;

    unsigned int packetLen = p - message;
    int rc = sendToDirectoryServer(addr, message, packetLen);
    
    cout << "ONLINE Message:" << endl;
    prettyHexDump(message, packetLen, cout);

    return 0;
}

int main(int, const char**) {

    const char* callSign = "KC1FSZ";
    const char* fullName = "Bruce R. MacKinnon";
    const char* password = "echolink666";
    const char* location = "Wellesley, MA USA";

    // Try to login 
    sendOnline(callSign, password, location);

    int rc;
    int rtpPort = 5198;
    int rtcpPort = 5199;
    int rtpSock, rtcpSock;

    struct sockaddr_in rtp_local_addr;
    struct sockaddr_in rtp_remote_addr;
    struct sockaddr_in rtcp_local_addr;
    struct sockaddr_in rtcp_remote_addr;

    rtpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtpSock < 0) {
        cout << "Socket problem" << endl;
        return -1;
    }

    memset(&rtp_local_addr, 0, sizeof(sockaddr_in));
    rtp_local_addr.sin_family = AF_INET;
    rtp_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rtp_local_addr.sin_port = htons(rtpPort);

    rc = bind(rtpSock, (struct sockaddr*)&rtp_local_addr, 
        sizeof(sockaddr_in));
    if (rc < 0) {
        cout << "Bind problem" << endl;
        return -1;
    }

    rtcpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtcpSock < 0) {
        cout << "Socket problem" << endl;
        return -1;
    }

    memset(&rtcp_local_addr, 0, sizeof(sockaddr_in));
    rtcp_local_addr.sin_family = AF_INET;
    rtcp_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rtcp_local_addr.sin_port = htons(rtcpPort);

    rc = bind(rtcpSock, (struct sockaddr*)&rtcp_local_addr, 
        sizeof(sockaddr_in));
    if (rc < 0) {
        cout << "Bind problem" << endl;
        return -1;
    }

    // Get the remote addresses setup
    const char* echoTestAddr = "54.176.61.77";

    memset(&rtp_remote_addr, 0, sizeof(sockaddr_in));
    rtp_remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, echoTestAddr, &(rtp_remote_addr.sin_addr));
    rtp_remote_addr.sin_port = htons(rtpPort);

    memset(&rtcp_remote_addr, 0, sizeof(sockaddr_in));
    rtcp_remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, echoTestAddr, &(rtcp_remote_addr.sin_addr));
    rtcp_remote_addr.sin_port = htons(rtcpPort);

    // Sockets mon-blocking
    rc = fcntl(rtpSock, F_SETFL, O_NONBLOCK); 
    if (rc < 0) {
        cout << "fcntl problem" << endl;
        return -1;
    }
    rc = fcntl(rtcpSock, F_SETFL, O_NONBLOCK); 
    if (rc < 0) {
        cout << "fcntl problem" << endl;
        return -1;
    }

    // Fire off the initial RTCP message
    const uint32_t packetSize = 256;
    uint8_t p[packetSize];
    uint32_t ssrc = 0;
    //uint32_t ssrc2 = 0x90d7678d;
    //uint32_t ssrc2 = 0xb09e44a6;
    uint32_t ssrc2 = 0x12782a48;
    uint32_t len;

    const uint32_t pcmDataSize = 160 * 2048;
    int16_t pcmData[pcmDataSize];
    uint32_t pcmPtr = 0;
    kc1fsz::Decoder gsmDecoder;  
    uint32_t rtpCount = 0;
    uint32_t rtcpCount = 0;
    int state = 0;
    uint32_t stateStamp = 0;
    uint32_t keepAliveStamp = 0;

    // Save every 10 seconds
    uint32_t saveMs = time_ms() + 10000;
    int saveCount = 0;

    // ------ EVENT LOOP ---------------------------------------------------

    while (true) {

        // In state 0 we are waiting to initiate the connection
        if (state == 0) {
            sendHello(rtpSock, rtcpSock, rtp_remote_addr, rtcp_remote_addr, 
                callSign, fullName, ssrc, ssrc2, false);
            // Waiting for data
            state = 1;
            stateStamp = time_ms() + 3000;
        } 
        // In state 1 we are waiting for the initiation to be acknowledged
        else if (state == 1) {
            if (time_ms() > stateStamp) {
                state = 0;
            }
        }
        else if (state == 2) {
            // Watch for keep-alive
            if (time_ms() > keepAliveStamp + 10000) {
                sendHello(rtpSock, rtcpSock, rtp_remote_addr, rtcp_remote_addr, 
                    callSign, fullName, ssrc, ssrc2, true);
                keepAliveStamp = time_ms();
            }
        }

        // Take whatever PCM data has been collected and save it
        if (time_ms() > saveMs) {
            if (pcmPtr > 0) {
                char out_fn[256];
                sprintf(out_fn, "../tmp/qso-%02d.wav", saveCount);
                cout << "Writing " << out_fn << endl;
                std::ofstream out_file(out_fn, std::ios::binary);
                if (!out_file.good()) {
                    assert(false);
                }
                kc1fsz::encodeFromPCM16(pcmData, pcmPtr, out_file, 8000);
                out_file.close();
                pcmPtr = 0;
            }
            saveMs = time_ms() + 10000;
            saveCount++;
        }

        fd_set readfds; 
        FD_ZERO(&readfds);
        FD_SET(rtpSock, &readfds);
        FD_SET(rtcpSock, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        int rv = select(rtcpSock + 1, &readfds, NULL, NULL, &tv); 
        if (rv > 0) {

            if (FD_ISSET(rtpSock, &readfds)) {
                len = recvfrom(rtpSock, p, packetSize, 0, 0, 0);
                if (len > 0) {
                    rtpCount++;                
                    state = 2;
                    if (isOnDataPacket(p, len)) {
                        cout << rtpCount << " Received oNDATA" << endl;
                        prettyHexDump(p, len, cout);
                    } else if (isRTPPacket(p, len)) {
                        cout << rtpCount << " Received audio" << endl;
                        // Translate frame into PCM data
                        uint8_t gsmFrames[4][33];
                        uint16_t remoteSeq = 0;
                        uint32_t remoteSSRC = 0;
                        parseRTPPacket(p, &remoteSeq, &remoteSSRC, gsmFrames);
                        kc1fsz::Parameters params;
                        for (uint32_t f = 0; f < 4; f++) {
                            kc1fsz::PackingState state;
                            params.unpack(gsmFrames[f], &state);
                            gsmDecoder.decode(&params, pcmData + pcmPtr);
                            pcmPtr += 160;
                            if (pcmPtr >= pcmDataSize) {
                                cout << "PCM WRAPPED!" << endl;
                                pcmPtr = 0;
                            }
                        }
                    } else {
                        cout << rtpCount << " Unrecognized" << endl;
                        prettyHexDump(p, len, cout);
                    }
                }
            }

            if (FD_ISSET(rtcpSock, &readfds)) {
                len = recvfrom(rtcpSock, p, packetSize, 0, 0, 0);
                if (len > 0) {
                    rtcpCount++;                
                    state = 2;
                    cout << rtcpCount << " Received RTCP" << endl;
                    prettyHexDump(p, len, cout);
                }
            }
        }
    }

    // Send a bye
    len = formatRTCPPacket_BYE(ssrc, p, packetSize);
    cout << "RTCP BYE packet (length=" << len << "):" << endl;
    prettyHexDump(p, len, cout);
}
