#pragma once

#include "types.h"

class BaseDisplay
{
protected:
    virtual ~BaseDisplay() {}
public:
    virtual void init() = 0;
    virtual void complete() {}
    virtual void fill(const Rect &rect, RGB565 colour) = 0;
    virtual void copy(const Rect &rect, const RGB565 *src) = 0;
    virtual void copy1bit(const Point &pt, int pixels, const uint8_t *src, int x_offset, RGB565 bg, RGB565 fg) = 0;
};
