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

#include "common.h"
#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/wav_util.h"

using namespace std;
using namespace kc1fsz;

static int char2int(char input) {
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    throw std::invalid_argument("Invalid input string");
}

/**
 * Converts an encoded string in af05cb0302a4.... format (bytes in ASCII hex 
 * representation) to an array of bytes.  It is assumed that the string
 * is null-terminated.
*/
static uint32_t parseHex(const char* src, uint8_t* target, uint32_t targetLen) {
    // There is an intentional round-down going on here
    uint32_t hexBytes = std::min((uint32_t)std::strlen(src) / 2, targetLen);
    for (uint32_t i = 0; i < hexBytes; i++) {
        *(target++) = char2int(*src) * 16 + char2int(src[1]);
        src += 2;
    }
    return hexBytes;
}


static const uint32_t pcmSize = 160 * 8192;
static int16_t pcm[pcmSize];

static void test_1() {

    string inp_fn = "../tests/data/qso-1.txt";
    std::ifstream inp_file(inp_fn, std::ios::binary);
    if (!inp_file.good()) {
        assert(false);
    }
    
    // Setup a GSM decoder
    kc1fsz::Decoder decoder;

    std::string line;
    unsigned int packetCount = 0;
    unsigned int pcmPtr = 0;

    while (getline(inp_file, line)) {
        
        rtrim(line);
        uint8_t packet[1024];
        uint32_t packetLen = parseHex(line.c_str(), packet, 1024);

        if (isOnDataPacket(packet, packetLen)) {
            cout << "oNDATA" << endl;
        } 
        else if (isRTPPacket(packet, packetLen)) {

            // Unpack the RTP packet
            uint16_t seq;
            uint32_t ssrc;
            uint8_t gsmFrames[4][33];
            parseRTPPacket(packet, &seq, &ssrc, gsmFrames);

            // Only listening to one side
            if (ssrc != 0) {
                // Feed the four GSM frames into the decoder
                for (uint16_t i = 0; i < 4; i++) {
                    // Unpack the parameters
                    kc1fsz::PackingState state;
                    kc1fsz::Parameters parms;
                    if (!kc1fsz::Parameters::isValidFrame(gsmFrames[i])) {
                        assert(false);
                    }
                    // TODO: MAKE A VERSION WITH THE STATE INTERNALIZED
                    parms.unpack(gsmFrames[i], &state);
                    if (pcmPtr < pcmSize) {
                        // Decode to PCM
                        decoder.decode(&parms, &(pcm[pcmPtr]));
                        pcmPtr += 160;
                    }
                }
            }
        } else {
            cout << "Unknown packet type at " << packetCount << endl;
        }

        packetCount++;
    }

    inp_file.close();

    // Dump the PCM sames into a .WAV
    string out_fn = "../tmp/qso-1.wav";
    std::ofstream out_file(out_fn, std::ios::binary);
    if (!out_file.good()) {
        assert(false);
    }
    kc1fsz::encodeFromPCM16(pcm, pcmPtr, out_file, 8000);
    out_file.close();

    cout << "Packets: " << packetCount << endl;
    cout << "PCM samples: " << pcmPtr << endl;
}

int main(int, const char**) {
    test_1();
}
