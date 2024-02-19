#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gsm-0610-codec/wav_util.h"
#include "gsm-0610-codec/Encoder.h"
#include "gsm-0610-codec/Parameters.h"

#include "Prompts.h"

using namespace std;
using namespace kc1fsz;

const uint32_t samplesSize = 1024 * 1024;
int16_t samples[samplesSize];

int main(int, const char**) {

    ifstream inf("../prompts/Prompts1.wav");
    int s = decodeToPCM16(inf, samples, samplesSize);
    cout << s << endl;

    ifstream in_pos("../prompts/pos.txt");
    std::string line;
    std::vector<int> starts;
    std::vector<int> lengths;

    while (std::getline(in_pos, line)) {
        // Split by tab
        size_t d = line.find("\t");
        if (d == string::npos) {
            continue;
        }
        string code = line.substr(0, d);
        string pos = line.substr(d + 1);
        cout << code << " -> " << pos << endl;
        starts.push_back(std::atoi(pos.c_str()));
    }

    for (int i = 0; i < starts.size(); i++) {
        int len = 0;
        if (i < starts.size() - 1) {
            len = starts.at(i + 1) - starts.at(i);
        } else {
            len = s - starts.at(i);
        }
        lengths.push_back(len);
    }

    int totalFrames = 0;
    uint8_t gsmData[2000 * 33];
    int gsmFramePtr = 0;

    Sound sounds[39];

    uint8_t *gsmPtr = gsmData;
    
    for (int i = 0; i < starts.size(); i++) {

        // Extract each sound
        int16_t pcm[160 * 48];
        for (int k = 0; k < 160 * 48; k++) {
            pcm[k] = 0;
        }
        for (int k = 0; k < lengths.at(i); k++) {
           pcm[k] = samples[starts.at(i) + k];
        }

        // Measure whole frames
        int frames = lengths.at(i) / 160;
        if (lengths.at(i) % 160 != 0) {
            frames++;
        }

        Encoder encoder;
        int16_t* pcmPtr = pcm;

        // Start
        sounds[i].start = totalFrames;
        sounds[i].length = frames;

        for (int f = 0; f < frames; f++) {
            Parameters params;
            encoder.encode(pcmPtr, &params);
            PackingState state;
            params.pack(gsmPtr, &state);
            pcmPtr += 160;
            gsmPtr += 33;
        }

        totalFrames += frames;
    }
    
    // Output structures
    /*
    for (int i = 0; i < 39; i++) {
        cout << "{" << sounds[i].start << ", " << sounds[i].length << "}, " << endl;
    }
    */

    gsmPtr = gsmData;

    ofstream outf("../prompts/out.txt");

    for (int i = 0; i < totalFrames; i++) {
        for (int k = 0; k < 33; k++) {
            outf << (int)(*gsmPtr) << ", ";
            gsmPtr++;
        }
        outf << endl;
    }
    
    outf.close();
}

