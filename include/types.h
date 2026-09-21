#pragma once

#include <cstdint>
#include <cmath>

struct RGB565
{
    uint16_t value = 0;
    
    static inline constexpr RGB565 from565(uint8_t r, uint8_t g, uint8_t b) {
        return RGB565((uint16_t)(b | (uint16_t(g) << 5) | (uint16_t(r) << 11)));
    }

    static inline constexpr RGB565 rgb888(uint8_t r, uint8_t g, uint8_t b)
    {
        r >>= 3;
        g >>= 2;
        b >>= 3;
        uint16_t value = (r << 11) | (g << 5) | b;
        return RGB565(value);
    }
    static inline constexpr RGB565 rgb888(uint32_t rgb)
    {
        // In the compiler's optimization skills we trust (for now)
        uint16_t r = (rgb >> (16 + 3)) & 0x1F;
        uint16_t g = (rgb >> (8 + 2)) & 0x3F;
        uint16_t b = (rgb >> 3) & 0x1F;
        uint16_t value = (r << 11) | (g << 5) | b;
        return RGB565(value);
    }

    RGB565() = default;
    explicit constexpr RGB565(uint16_t _value)
    : value(_value)
    {}
    
    inline uint16_t val() const { return value; }
    inline uint8_t r8() const { return (value >> 11) << 3; }
    inline uint32_t r5() const { return (value >> 11); }
    inline uint8_t g8() const { return ((value >> 5) & 0x3F ) << 2; }
    inline uint32_t g6() const { return (value >> 5) & 0x3F; }
    inline uint8_t b8() const { return (value & 0x1F) << 3; }
    inline uint32_t b5() const { return value & 0x1F; }    
    inline float luma() const {
        return 0.299f * r8() + 0.587f * g8() + 0.114f * b8();
    }
};

struct Point
{
    int16_t x = 0;
    int16_t y = 0;
    
    Point() = default;
    Point(int16_t _x, int16_t _y) : x(_x), y(_y) {}
};

struct Rect
{
    Point tl;
    Point br; // non-inclusive

    Rect() = default;
    Rect(int xs, int ys, int xe, int ye) : tl(xs, ys), br(xe, ye) {}
    int16_t left() const { return tl.x; }
    int16_t right() const { return br.x; }
    int16_t right_inc() const { return br.x - 1; }
    int16_t top() const { return tl.y; }
    int16_t bottom() const { return br.y; }
    int16_t bottom_inc() const { return br.y - 1; }
    int16_t width() const { return br.x - tl.x; }
    int16_t height() const { return br.y - tl.y; }
    bool empty() const { return tl.x >= br.x || tl.y >= br.y; }
    Rect intersection(const Rect &r2) const { return Rect(std::max(tl.x, r2.tl.x), std::max(tl.y, r2.tl.y), std::min(br.x, r2.br.x), std::min(br.y, r2.br.y)); }
};

struct Palette16
{
    RGB565 cmap[16];
    int reserve;

    Palette16(RGB565 bg, RGB565 fg, int _reserve = 0)
    : reserve(_reserve)
    {
        cmap[0] = bg;
        cmap[15 - reserve] = fg;
        float luma1 = fg.luma() / 255.0;
        float luma2 = bg.luma() / 255.0;
        // This is NOT a correct calculation. Looks somewhat OKish though.
        float gamma = powf(1.5, luma2 - luma1);
        for (int i = 1; i < 15 - reserve; ++i) {
            unsigned fgv = 256 * powf(i / (15.0 - reserve), gamma);
            unsigned bgv = 256 - fgv;
            cmap[i] = RGB565::rgb888(
                (bg.r8() * bgv + fg.r8() * fgv) >> 8,
                (bg.g8() * bgv + fg.g8() * fgv) >> 8,
                (bg.b8() * bgv + fg.b8() * fgv) >> 8);
        }
    }
    void setColour(int slot, RGB565 colour)
    {
        cmap[15 - slot] = colour;
    }
    RGB565 fg() const {
        return cmap[15 - reserve];
    }
    RGB565 bg() const {
        return cmap[0];
    }
};
