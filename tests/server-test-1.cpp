
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

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"

using namespace std;
using namespace kc1fsz;

static const char* HELLO_MSG =
"-----BEGIN PUBLIC KEY-----\n"
"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC3Ch/tc4yHC7bbXbjisSJO+HJr\n"
"TdLzMYEAdXbpHLk4CZVe7gTGD7XIX3TPy3D1f1KqcdN8c43ct5P/iHjgv8KpVA0k\n"
"pNUilebz5rKiBAMTeLT65IjD71SnxRt9iPDpja9vbhx2XPnjR7CoNBdtl/aY65kF\n"
"G1z0ZUIsoFdAAhFloQIDAQAB\n"
"-----END PUBLIC KEY-----\n"
"WQ25EC5c1yOh2MI+gnyDlA==\n";

static void service(int connfd)  { 

    const uint32_t buffSize = 256;
    char buff[buffSize]; 
    int len; 

    // Send out the initial message
    len = write(connfd, HELLO_MSG, strlen(HELLO_MSG));

    cout << "Sent" << endl;
    prettyHexDump((const uint8_t*)HELLO_MSG, len, cout);

    // infinite loop for chat 
    for (;;) { 
   
        // read the message from client and copy it in buffer 
        len = read(connfd, buff, buffSize); 
        if (len > 0) {
            cout << "Received" << endl;
            prettyHexDump((const uint8_t*)buff, len, cout);
            //cout << "Received Raw" << endl;
            //cout.write(buff, len);
            //cout << endl;
        }
    } 
} 

int main(int, const char**) {

    int sockfd, connfd, len; 
    struct sockaddr_in servaddr, cli; 
   
    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        cout << "socket creation failed" << endl;
        return 1;
    } 

    // assign IP, PORT 
    bzero(&servaddr, sizeof(servaddr));    
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(5200); 
   
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr))) != 0) { 
        cout << "socket bind failed" << endl;
        return -1;
    } 
   
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        cout << "Listen failed..." << endl;
        return -1;
    } 

    len = sizeof(cli);    
    connfd = accept(sockfd, (sockaddr*)&cli, &len); 
    if (connfd < 0) { 
        cout << "server accept failed" << endl;
        return -1;
    } 
   
    cout << "Got a connection" << endl;

    // Function for chatting between client and server 
    service(connfd); 
   
    // After chatting close the socket 
    close(sockfd); 
}
