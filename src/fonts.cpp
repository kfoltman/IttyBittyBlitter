#include "fonts.h"
#include "display.h"
#include <cstring>

int Font::textWidth(const char *text, int len)
{
    if (len == -1)
        len = strlen(text);
    int totalWidth = 0;
    for (int c = 0; c < len; ++c) {
        uint8_t ch = text[c];

        if (ch < firstChar || ch >= firstChar + charCount) {
            totalWidth += spaceWidth;
            continue;
        }
        int width = xs[ch - firstChar + 1] - xs[ch - firstChar];
        totalWidth += width;
    }
    return totalWidth;
}

void Font::drawText(BaseDisplay &disp, const Palette16 &palette, Point pt, const char *text, int len)
{
    if (len == -1)
        len = strlen(text);
    for (int c = 0; c < len; ++c) {
        uint8_t ch = text[c];
        if (ch < firstChar || ch >= firstChar + charCount) {
            disp.fill(Rect(pt.x, pt.y, pt.x + spaceWidth, pt.y + height), palette.bg());
            pt.x += spaceWidth;
            continue;
        }
        int pos = xs[ch - firstChar];
        int width = xs[ch - firstChar + 1] - pos;
        disp.copy4bit(pt, width, height, bits, pos, pitch, palette.cmap);
        pt.x += width;
    }
}

#include "fonts.inc"
