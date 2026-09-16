#pragma once

#include "types.h"

class BaseDisplay;

struct Font
{
public:
    const uint16_t *xs = nullptr;
    const uint8_t *bits = nullptr;

    uint8_t firstChar, charCount;
    uint8_t height;
    uint8_t spaceWidth;
    uint16_t pitch;

    int textWidth(const char *text, int len = -1);
    void drawText(BaseDisplay &disp, const Palette16 &palette, Point pt, const char *text, int len = -1);
};

extern Font font_small;
