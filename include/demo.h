#pragma once

#include "fonts.h"
#include "display.h"

class Demo
{
public:
    Palette16 palette;
    int t = 0;
    uint32_t startTime = 0;

    void init(BaseDisplay &display);
    void loop(BaseDisplay &display, uint32_t millis);
};