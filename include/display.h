#pragma once

#include "types.h"

class BaseDisplay
{
protected:
    int16_t w, h;
    BaseDisplay(int16_t _w, int16_t _h) : w(_w), h(_h) {}
    virtual ~BaseDisplay() {}
public:
    inline int16_t width() const { return w; }
    inline int16_t height() const { return h; }
    virtual void init() = 0;
    virtual void complete() {}
    virtual void fill(const Rect &rect, RGB565 colour) = 0;
    virtual void copy(const Rect &rect, const RGB565 *src) = 0;
    virtual void copy1bit(const Point &pt, int pixels, const uint8_t *src, int x_offset, int pitch, RGB565 bg, RGB565 fg) = 0;
};
