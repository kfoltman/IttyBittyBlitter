#pragma once

#include "types.h"
#include <algorithm>

class BaseDisplay
{
protected:
    int16_t w, h;
    Rect clip;
    virtual ~BaseDisplay() {}
public:
    BaseDisplay(int16_t _w, int16_t _h)
    : w(_w)
    , h(_h)
    , clip(0, 0, _w, _h) {}
    inline int16_t width() const { return w; }
    inline int16_t height() const { return h; }
    void setClipRect(const Rect &cr) {
        clip = Rect(std::max<int16_t>(0, cr.left()), std::max<int16_t>(0, cr.top()), std::min<int16_t>(w, cr.right()), std::min<int16_t>(h, cr.bottom()));
    }
    const Rect &getClipRect() {
        return clip;
    }
    virtual void init() = 0;
    virtual void complete() {}
    virtual void fill(const Rect &rect, RGB565 colour) = 0;
    virtual void copy1bit(const Point &pt, int pixels, int height, const uint8_t *src, int x_offset, int pitch, RGB565 bg, RGB565 fg) = 0;
    virtual void copy4bit(const Point &pt, int pixels, int height, const uint8_t *src, int x_offset, int pitch, const RGB565 *cmap) = 0;
    virtual void copy16bit(const Point &pt, int width, int height, const uint16_t *src, int pitch) = 0;
};

template<typename T>
class BaseDisplayOps: public BaseDisplay
{
public:
    using BaseDisplay::BaseDisplay;
    template<typename Gen>
    static inline void output(BaseDisplayOps<T> *obj, const Rect &rect, Gen &gen)
    {
        static_cast<T*>(obj)->output(rect, gen);
    }
    void fill(const Rect &rect, RGB565 colour) {
        struct Solid {
            uint16_t val;
            inline uint16_t next() const { return val; }
            inline void line(int) const {}
            inline void skip(int) const {}
        };
        Solid solid{colour.val()};
        output(this, rect, solid);
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
        output(this, Rect(pt.x, pt.y, pt.x + pixels, pt.y + height), slurp);
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
        output(this, Rect(pt.x, pt.y, pt.x + pixels, pt.y + height), slurp);
    }
    void copy16bit(const Point &pt, int width, int height, const uint16_t *src, int pitch) {
        struct Slurp {
            const uint16_t *src;
            int pos;
            int pitch;
            inline uint16_t next() {
                return src[pos++];
            }
            inline void skip(int count) {
                pos += count;
            }
            inline void line(int count) {
                src += count * pitch;
                pos = 0;
            }
        };
        Slurp slurp{src, 0, pitch};
        output(this, Rect(pt.x, pt.y, pt.x + width, pt.y + height), slurp);
    }
};

class BufferDisplay: public BaseDisplayOps<BufferDisplay>
{
protected:
    uint16_t *data;
public:
    BufferDisplay(uint16_t *_data, int _width, int _height)
    : BaseDisplayOps<BufferDisplay>(_width, _height)
    , data(_data)
    {}
    void init() {}
    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.empty())
            return;
        Rect clipped = rect.intersection(this->clip);
        if (clipped.empty())
            return;
        int xl = clipped.left(), xr = clipped.right();
        int yt = clipped.top(), yb = clipped.bottom();

        if (yt > rect.top())
            gen.line(yt - rect.top());
        for (int y = yt; y < yb; y++) {
            if (xl > rect.left())
                gen.skip(xl - rect.left());
            uint16_t *wptr = &data[y * w + xl];
            for (int x = xl; x < xr; ++x)
                *wptr++ = gen.next();
            gen.line(1);
        }
    }
};

template<class ChunkInterface, int Width, int Height>
class ChunkOutputDisplay: public BaseDisplayOps<ChunkOutputDisplay<ChunkInterface, Width, Height> >
{
protected:
    volatile uint16_t *data; // must point to DMAable memory
    uint32_t size, limit;
    volatile uint32_t read_ptr, write_ptr;
protected:
    int availableToWrite() const {
        // Cannot distinguish between full and empty with two pointers, so 1 byte will get wasted.
        return (read_ptr <= write_ptr) ? (size - 1 - write_ptr + read_ptr) : (read_ptr - write_ptr - 1);
    }
    inline void write(uint16_t value) {
        data[write_ptr++] = value;
        if (write_ptr == size)
            write_ptr = 0;
    }
    inline uint16_t read() {
        uint16_t value = data[read_ptr++];
        if (read_ptr == size)
            read_ptr = 0;
        return value;
    }
    inline void writeArea(int xl, int yt, int xr, int yb) {
        write(xl);
        write(yt);
        write(xr);
        write(yb);
    }
    void trySendChunk() {
        if (read_ptr != write_ptr) {
            if (canSendChunk()) {
                int xl = read();
                int yt = read();
                int xr = read();
                int yb = read();
                sendChunk(xl, yt, xr, yb);
            } else {
                idle();
            }
        }
    }
    bool canSendChunk() {
        if (ChunkInterface::busy())
            return false;
        if (read_ptr == write_ptr)
            return false;
        return true;
    }
    void sendChunk(int xl, int yt, int xr, int yb) {
        ChunkInterface::writeArea(xl, yt, xr, yb);
        uint32_t words = (xr - xl) * (yb - yt);
        if (read_ptr + words > size) {
            uint32_t span = size - read_ptr;
            uint32_t rest = words - span;
            ChunkInterface::pixels(&data[read_ptr], span, [this, rest] {
                read_ptr = 0;
                ChunkInterface::pixels(&data[0], rest, [this, rest] {
                    read_ptr = rest;
                    trySendChunk();
                });
            });
            return;
        }
        ChunkInterface::pixels(&data[read_ptr], words, [this, words] {
            read_ptr = (read_ptr + words) % size;
            trySendChunk();
        });
    }
public:
    using DisplayInterface = ChunkInterface;

    ChunkOutputDisplay(uint16_t *_data, uint32_t _size)
    : BaseDisplayOps<ChunkOutputDisplay<ChunkInterface, Width, Height>>(Width, Height)
    , data{_data}
    , size{_size}
    , read_ptr{0}
    , write_ptr{0}
    {
        // 64K DMA limit or not more than 25% of the buffer,
        // including the header
        limit = std::min<uint32_t>(size / 4 - 5, 65535);
    }
    void init() {
    }
    virtual void idle() {
        ChunkInterface::idle();
    }
    void complete() {
        while(read_ptr != write_ptr)
            trySendChunk();
    }

    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.empty())
            return;
        Rect clipped = rect.intersection(this->clip);
        if (clipped.empty())
            return;
        int xl = clipped.left(), xr = clipped.right();
        int yt = clipped.top(), yb = clipped.bottom();

        if (yt > rect.top())
            gen.line(yt - rect.top());
        for (int y0 = yt; y0 < yb; ) {
            int space = availableToWrite() - 4;
            if (space > (int)limit)
                space = limit;
            int max_lines = space / (xr - xl);
            if (max_lines < 1) {
                trySendChunk();
                continue;
            }
            int ye = std::min(y0 + max_lines, yb);
            writeArea(xl, y0, xr, ye);
            for (int y = y0; y < ye; ++y) {
                if (xl > rect.left())
                    gen.skip(xl - rect.left());
                for (int x = xl; x < xr; ++x)
                    write(gen.next());
                gen.line(1);
            }
            y0 = ye;
        }
        trySendChunk();
    }
};

extern void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg);
extern void triangle(BaseDisplay &disp, Point p1, Point p2, Point p3, RGB565 fg);
