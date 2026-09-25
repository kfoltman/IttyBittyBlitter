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

void Font::drawPaddedText(BaseDisplay &disp, const Palette16 &palette, Rect rc, uint32_t flags, const char *text, int len)
{
    if (len == -1)
        len = strlen(text);
    int width = textWidth(text, len);
    int hpos = 0;
    uint32_t halign = flags & DT_HALIGN_MASK;
    if (halign == DT_RIGHT)
        hpos = std::max(0, rc.width() - width);
    if (halign == DT_CENTRE)
        hpos = std::max(0, (rc.width() - width) / 2);
    if (width > rc.width() || height > rc.height()) {
        Rect oldClip = disp.getClipRect();
        disp.setClipRect(oldClip.intersection(rc));
        drawText(disp, palette, Point(rc.tl.x + hpos, rc.tl.y), text, len);
        disp.setClipRect(oldClip);
    } else {
        drawText(disp, palette, Point(rc.tl.x + hpos, rc.tl.y), text, len);
    }
    if (height < rc.height()) {
        disp.fill(Rect(rc.left(), rc.top() + height, rc.right(), rc.bottom()), palette.bg());
    }
    if (width < rc.width()) {
        if (halign == DT_RIGHT)
            disp.fill(Rect(rc.left(), rc.top(), rc.right() - width, rc.top() + height), palette.bg());
        else if (halign == DT_CENTRE) {
            disp.fill(Rect(rc.left(), rc.top(), rc.left() + hpos, rc.top() + height), palette.bg());
            disp.fill(Rect(rc.left() + hpos + width, rc.top(), rc.right(), rc.top() + height), palette.bg());
        }
        else
            disp.fill(Rect(rc.left() + width, rc.top(), rc.right(), rc.top() + height), palette.bg());
    }
}

#include "fonts.inc"
