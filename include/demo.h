#pragma once

#include "fonts.h"
#include "display.h"

class Demo
{
public:
    Palette16 palette;
    int t = 0;
    uint32_t totalTime = 0;

    void init();
    void loop(BaseDisplay &display, uint32_t millis);
};