#include "IttyBittyBlitter/display.h"
#include <cassert>

//////////////////////////////////////////////////////////////////////////////////////////////////

void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg)
{
    int ys = std::max<int16_t>(0, yc - r);
    int ye = std::min<int16_t>(disp.height(), yc + r + 1);
    int w = disp.width();

    int r2minuss2 = r * r;
    int c = r, c2 = r2minuss2;
    int l = (2896 * r) >> 12; // approx. sqrt(2) / 2
    for (int s = 0; s < r; s++) {
        if (c2 > r2minuss2)
            c2 = c2 - 2 * (c--) + 1;
        if (c < s) // stop after a full octant
            break;
        int xr = c;
        int xleft = std::max<int16_t>(0, xc - xr);
        int xright = std::min<int16_t>(xc + xr + 1, w);
        disp.fill(Rect(xleft, yc - s, xright, yc - s + 1), fg);
        disp.fill(Rect(xc - s, yc - c, xc - s + 1, yc - l), fg);
        disp.fill(Rect(xc - s, yc + l, xc - s + 1, yc + c + 1), fg);
        if (s) {
            disp.fill(Rect(xleft, yc + s, xright, yc + s + 1), fg);
            disp.fill(Rect(xc + s, yc - c, xc + s + 1, yc - l), fg);
            disp.fill(Rect(xc + s, yc + l, xc + s + 1, yc + c + 1), fg);
        }
        r2minuss2 = r2minuss2 - 2 * s + 1;
    }
}

void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg, RGB565 bg)
{
    int ys = std::max<int16_t>(0, yc - r);
    int ye = std::min<int16_t>(disp.height(), yc + r + 1);
    int w = disp.width();

    int l = (2896 * r) >> 12; // approx. sqrt(2) / 2
    for (int s = 0; s < r; s++) {
        float cf = sqrtf(r * r - s * s);
        int c = floor(cf);
        if (c < s) // stop after a full octant
            break;
        int xleft = std::max<int16_t>(0, xc - c);
        int xright = std::min<int16_t>(xc + c + 1, w);
        uint8_t alpha = 16 * (cf - c);
        int shift = 1;
        disp.fill(Rect(xleft + shift, yc - s, xright - shift, yc - s + 1), fg);
        disp.fill(Rect(xc - s, yc - c + shift, xc - s + 1, yc - l), fg);
        disp.fill(Rect(xc - s, yc + l, xc - s + 1, yc + c + 1 - shift), fg);
        if (s) {
            disp.fill(Rect(xleft + shift, yc + s, xright - shift, yc + s + 1), fg);
            disp.fill(Rect(xc + s, yc - c + shift, xc + s + 1, yc - l), fg);
            disp.fill(Rect(xc + s, yc + l, xc + s + 1, yc + c + 1 - shift), fg);
        }
        if (alpha) {
            RGB565 mixed = RGB565::blend(bg, fg, alpha);
            disp.fill(Rect(xleft, yc - s, xleft + 1, yc - s + 1), mixed);
            disp.fill(Rect(xright - 1, yc - s, xright, yc - s + 1), mixed);
            disp.fill(Rect(xleft, yc + s, xleft + 1, yc + s + 1), mixed);
            disp.fill(Rect(xright - 1, yc + s, xright, yc + s + 1), mixed);
            disp.fill(Rect(xc - s, yc - c, xc - s + 1, yc - c + 1), mixed);
            disp.fill(Rect(xc + s, yc - c, xc + s + 1, yc - c + 1), mixed);
            disp.fill(Rect(xc - s, yc + c, xc - s + 1, yc + c + 1), mixed);
            disp.fill(Rect(xc + s, yc + c, xc + s + 1, yc + c + 1), mixed);
        }
    }
}



void triangle(BaseDisplay &disp, Point p1, Point p2, Point p3, RGB565 fg)
{
    if (p3.y < p2.y)
        std::swap(p2, p3);
    if (p2.y < p1.y)
        std::swap(p1, p2);
    if (p3.y < p1.y)
        std::swap(p1, p3);
    if (p3.y < p2.y)
        std::swap(p2, p3);
    assert(p1.y <= p2.y);
    assert(p2.y <= p3.y);
    auto scanline = [&disp,fg](float x1, float x2, int y) {
        disp.fill(Rect(roundf(std::min(x1, x2)), y, roundf(std::max(x1, x2)) + 1, y + 1), fg);
    };
    if (p1.y == p2.y) {
        if (p2.y == p3.y)
            return;
        for (int y = p1.y; y <= p3.y; ++y) {
            float x1 = p1.x + float(p3.x - p1.x) * (y - p1.y) / (p3.y - p1.y);
            float x2 = p2.x + float(p3.x - p2.x) * (y - p1.y) / (p3.y - p1.y);
            scanline(x1, x2, y);
        }
    } else if (p2.y == p3.y) {
        for (int y = p1.y; y <= p2.y; ++y) {
            float x1 = p1.x + float(p3.x - p1.x) * (y - p1.y) / (p3.y - p1.y);
            float x2 = p1.x + float(p2.x - p1.x) * (y - p1.y) / (p3.y - p1.y);
            scanline(x1, x2, y);
        }
    } else {
        for (int y = p1.y; y < p2.y; ++y) {
            float x1 = p1.x + float(p2.x - p1.x) * (y - p1.y) / (p2.y - p1.y);
            float x2 = p1.x + float(p3.x - p1.x) * (y - p1.y) / (p3.y - p1.y);
            scanline(x1, x2, y);
        }
        for (int y = p2.y; y <= p3.y; ++y) {
            float x1 = p2.x + float(p3.x - p2.x) * (y - p2.y) / (p3.y - p2.y);
            float x2 = p1.x + float(p3.x - p1.x) * (y - p1.y) / (p3.y - p1.y);
            scanline(x1, x2, y);
        }
    }
}
