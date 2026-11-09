#pragma once

#include "types.h"
#include <algorithm>
#include <atomic>

#ifdef DESKTOP_SIM
#include <cassert>
#include <cstdio>
#define pcassert assert
#else
#define pcassert(...)
#endif

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
    const Rect &getClipRect() const {
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

template<class ChunkInterface>
class ChunkOutputDisplay: public BaseDisplayOps<ChunkOutputDisplay<ChunkInterface> >
{
protected:
    volatile uint16_t *data; // must point to DMAable memory
    uint32_t size, limit;
    std::atomic_int read_ptr, write_ptr;
    uint32_t tmp_read_ptr, tmp_write_ptr;
    std::atomic_bool busy;
protected:
    int availableToWrite() const {
        // Cannot distinguish between full and empty with two pointers, so 1 byte will get wasted.
        return (read_ptr <= write_ptr) ? (size - 1 - write_ptr + read_ptr) : (read_ptr - write_ptr - 1);
    }
    inline void write(uint16_t value) {
        data[tmp_write_ptr++] = value;
        if (tmp_write_ptr == size) {
            tmp_write_ptr = 0;
        }
    }
    inline uint16_t read() {
        uint16_t value = data[tmp_read_ptr];
        tmp_read_ptr = (tmp_read_ptr == size - 1) ? 0 : tmp_read_ptr + 1;
        return value;
    }
    inline void writeArea(int xl, int yt, int xr, int yb) {
        pcassert(xl != xr);
        pcassert(yt != yb);
        write(xl);
        write(yt);
        write(xr);
        write(yb);
    }
private:
    void trySendChunk() {
        // This is a very delicate scheme, because this runs concurrently
        // with the DMA or second thread pulling buffers. Write_ptr is constant,
        // but read_ptr can change at any point if busy == true.
        bool old_busy = busy;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (read_ptr.load() != write_ptr.load()) {
            if (!busy.exchange(true)) {
                // *Now* read_ptr won't change. But it could have changed since the last 'if'
                // if there was a transaction in progress at that point (old_busy).
                if (!old_busy || read_ptr.load() != write_ptr.load()) {
                    sendChunk();
                } else {
                    // We know that the flag was set in this function (via the exchange method).
                    // Nothing else should be accessing it at the moment.
                    busy = false;
                }
            } else {
                idle();
            }
        }
    }
    void trySendNextChunk() {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (read_ptr != write_ptr) {
            sendChunk();
        } else {
            // This must stay as true whenever there is a possibility for
            // read_ptr changing while the buffer gets written.
            busy = false;
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }
    void sendChunk() {
        int xl = read();
        int yt = read();
        int xr = read();
        int yb = read();
        pcassert(xl != xr);
        pcassert(yt != yb);
        ChunkInterface::writeArea(xl, yt, xr, yb);
        uint32_t words = (xr - xl) * (yb - yt);
        if (tmp_read_ptr + words > size) {
            uint32_t span = size - tmp_read_ptr;
            uint32_t rest = words - span;
            ChunkInterface::pixels(&data[tmp_read_ptr], span, [this, rest] {
                ChunkInterface::pixels(&data[0], rest, [this, rest] {
                    read_ptr = tmp_read_ptr = rest;
                    trySendNextChunk();
                });
            });
            return;
        }
        ChunkInterface::pixels(&data[tmp_read_ptr], words, [this, words] {
            read_ptr = tmp_read_ptr = (tmp_read_ptr + words) % size;
            trySendNextChunk();
        });
    }
public:
    using DisplayInterface = ChunkInterface;

    ChunkOutputDisplay(uint16_t *_data, uint32_t _size, int16_t _width, int16_t _height)
    : BaseDisplayOps<ChunkOutputDisplay<ChunkInterface>>(_width, _height)
    , data{_data}
    , size{_size}
    , read_ptr{0}
    , write_ptr{0}
    , tmp_read_ptr{0}
    , tmp_write_ptr{0}
    , busy{false}
    {
        ChunkInterface::init(_width, _height);
        // 64K DMA limit or not more than 25% of the buffer,
        // including the header, but it must fit at least
        // one full width line.
        limit = std::min<uint32_t>(std::max<uint32_t>(_width, size / 4 - 5), 65535);
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
            int space = availableToWrite() - 5;
            if (space > (int)limit)
                space = limit;
            int max_lines = space / (xr - xl);
            if (max_lines < 1) {
                // Assume that there are some buffers in circulation
                // already. It might look like it should not be necessary
                // to dispatch any chunks here, but due to a race between
                // read and write pointer. The scenario is as follows:
                // 1. The reader updates read_ptr
                // 2. The reader checks if read_ptr == write_ptr (and it is)
                // 3. The writer adds a new chunk, but does not send it
                //    because busy = true.
                // 4. The reader sets busy to false and goes idle
                // 5. There is an outstanding chunk but the reader is idle
                //    until re-awakened by (try)SendChunk
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
            std::atomic_thread_fence(std::memory_order_seq_cst);
            write_ptr = tmp_write_ptr;
            trySendChunk();
        }
    }
};

extern void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg);
extern void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg, RGB565 bg);
extern void triangle(BaseDisplay &disp, Point p1, Point p2, Point p3, RGB565 fg);
