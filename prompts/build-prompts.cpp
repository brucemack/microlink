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

struct Sound2 {
    char code;
    uint32_t start;
    uint32_t length;
};

int main(int, const char**) {

    ifstream inf("../prompts/Prompts1.wav");
    int s = decodeToPCM16(inf, samples, samplesSize);
    cout << s << endl;

    ifstream in_pos("../prompts/pos.txt");
    std::string line;
    std::vector<Sound2> sounds2;

    while (std::getline(in_pos, line)) {
        // Split by tab
        size_t d = line.find("\t");
        if (d == string::npos) {
            continue;
        }
        string code = line.substr(0, d);
        string rest = line.substr(d + 1);
        d = rest.find("\t");
        if (d == string::npos) {
            continue;
        }
        string pos = rest.substr(0, d);
        string len = rest.substr(d + 1);

        cout << code << " -> " << pos << " " << len << endl;

        Sound2 sound = { code.at(0), std::atoi(pos.c_str()), std::atoi(len.c_str()) };
        sounds2.push_back(sound);
    }

    int totalFrames = 0;
    uint8_t gsmData[2000 * 33];
    int gsmFramePtr = 0;
    uint8_t *gsmPtr = gsmData;
    
    std::vector<Sound> sounds;

    // For each symbol
    for (Sound2 sound : sounds2) {

        // Extract each sound
        int16_t pcm[160 * 48];
        for (int k = 0; k < 160 * 48; k++) {
            pcm[k] = 0;
        }
        for (int k = 0; k < sound.length; k++) {
           pcm[k] = samples[sound.start + k];
        }

        // Measure whole frames, rounding up
        int frames = sound.length / 160;
        if (sound.length % 160 != 0) {
            frames++;
        }

        Encoder encoder;
        int16_t* pcmPtr = pcm;

        for (int f = 0; f < frames; f++) {
            Parameters params;
            encoder.encode(pcmPtr, &params);
            PackingState state;
            params.pack(gsmPtr, &state);
            pcmPtr += 160;
            gsmPtr += 33;
        }

        Sound s = { sound.code, totalFrames, frames };
        sounds.push_back(s);

        totalFrames += frames;
    }
    
    // Output structures
    ofstream out1("../prompts/out1.txt");
    for (Sound s : sounds) 
        out1 << "{'" << s.code << "'," << s.start << ", " << s.length << "}, " << endl;
    out1.close();


    ofstream out2("../prompts/out2.txt");
    gsmPtr = gsmData;
    for (int i = 0; i < totalFrames; i++) {
        for (int k = 0; k < 33; k++) {
            out2 << (int)(*gsmPtr) << ", ";
            gsmPtr++;
        }
        out2 << endl;
    }
    out2.close();
}

