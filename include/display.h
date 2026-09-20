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
    virtual void copy1bit(const Point &pt, int pixels, int height, const uint8_t *src, int x_offset, int pitch, RGB565 bg, RGB565 fg) = 0;
    virtual void copy4bit(const Point &pt, int pixels, int height, const uint8_t *src, int x_offset, int pitch, const RGB565 *cmap) = 0;
};

template<typename T>
class BaseDisplayOps: public BaseDisplay
{
public:
    BaseDisplayOps(int16_t _w, int16_t _h) : BaseDisplay(_w, _h) {}
    void fill(const Rect &rect, RGB565 colour) {
        struct Solid {
            uint16_t val;
            inline uint16_t next() const { return val; }
            inline void line(int) const {}
            inline void skip(int) const {}
        };
        Solid solid{colour.val()};
        T::output(this, rect, solid);
    }
    void copy(const Rect &rect, const RGB565 *src) {
        struct Slurp {
            const RGB565 *src;
            int pos;
            int pitch;
            inline uint16_t next() { return src[pos++].val(); }
            inline void skip(int count) {
                pos += count;
            }
            inline void line(int count) {
                pos = 0;
                src += count * pitch;
            }
        };
        Slurp slurp{src, 0, rect.width()};
        T::output(this, rect, slurp);
    }
    void copy1bit(const Point &pt, int pixels, int height, const uint8_t *src, int x_offset, int pitch, RGB565 bg, RGB565 fg) {
        struct Slurp {
            const uint8_t *glyphs;
            int pos0, pos;
            int pitch;
            uint16_t bg;
            uint16_t fg;
            inline uint16_t next() {
                uint16_t rv = (glyphs[pos >> 3] << (pos & 7)) & 128 ? fg : bg;
                pos++;
                return rv;
            }
            inline void skip(int count) {
                pos += count;
            }
            inline void line(int count) {
                glyphs += count * pitch;
                pos = pos0;
            }
        };
        Slurp slurp{src, x_offset, x_offset, pitch, bg.val(), fg.val()};
        T::output(this, Rect(pt.x, pt.y, pt.x + pixels, pt.y + height), slurp);
    }
    void copy4bit(const Point &pt, int pixels, int height, const uint8_t *src, int x_offset, int pitch, const RGB565 *cmap) {
        struct Slurp {
            const uint8_t *glyphs;
            int pos0, pos;
            int pitch;
            const RGB565 *cmap;
            inline uint16_t next() {
                uint8_t alpha = (glyphs[pos >> 1] >> (((~pos) & 1) << 2)) & 0xF;
                uint16_t rv = cmap[alpha].val();
                pos++;
                return rv;
            }
            inline void skip(int count) {
                pos += count;
            }
            inline void line(int count) {
                glyphs += count * pitch;
                pos = pos0;
            }
        };
        Slurp slurp{src, x_offset, x_offset, pitch, cmap};
        T::output(this, Rect(pt.x, pt.y, pt.x + pixels, pt.y + height), slurp);
    }
};