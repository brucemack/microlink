#ifndef _Prompts_h
#define _Prompts_h

#include <cstdint>

struct Sound {

    char code;
    int start;
    uint32_t length;

    uint32_t getFrameCount() const { return length; }
    
    void getGSMFrame(int f, uint8_t* gsmData);

    static int findSound(char code);
};

extern Sound SoundMeta[];

#endif