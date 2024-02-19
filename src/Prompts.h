#ifndef _Prompts_h
#define _Prompts_h

#include <cstdint>

struct Sound {

    char code;
    int start;
    int length;

    int getFrameCount() const;

    void getFrame(int f, uint8_t* data);

    static int findSound(char code);
};

extern Sound SoundMeta[];
//extern uint8_t SoundData[];

#endif