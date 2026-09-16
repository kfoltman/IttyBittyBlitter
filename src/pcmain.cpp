#include <algorithm>
#include <functional>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <iostream>
#include "display.h"

class SDLDisplay: public BaseDisplay
{
private:
    int zoom;
    SDL_Surface *pc_display;
    Rect clip;
public:
    SDLDisplay(int _w, int _h, int _zoom = 1)
    : BaseDisplay(_w, _h)
    , zoom(_zoom)
    , clip(0, 0, _w, _h)
    {}
    void init() override {
        pc_display = SDL_SetVideoMode(w * zoom, h * zoom, 16, 0);
    }
    void complete() {
        SDL_Flip(pc_display);
    }
    void setClipRect(const Rect &cr) {
        clip = Rect(std::max<int16_t>(0, cr.left()), std::max<int16_t>(0, cr.top()), std::min<int16_t>(w, cr.right()), std::min<int16_t>(h, cr.bottom()));
    }
    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.width() < 0 || rect.height() < 0)
            return;
        int yt = std::max(rect.top(), clip.top());
        int yb = std::min(rect.bottom(), clip.bottom());
        if (yt >= yb)
            return;
        int xl = std::max(rect.left(), clip.left());
        int xr = std::min(rect.right(), clip.right());
        if (xl >= xr)
            return;
        int zw = xr - xl;
        if (!SDL_LockSurface(pc_display)) {
            uint8_t *pixels = (uint8_t *)pc_display->pixels;
            if (yt > rect.top())
                gen.line(yt - rect.top());
            for (int y = yt; y < yb; ++y) {
                if (xl > rect.left())
                    gen.skip(xl - rect.left());
                int ybase = y * zoom;
                uint16_t *dstbase = (uint16_t *)(pixels + pc_display->pitch * ybase);
                uint16_t *dst = dstbase + xl * zoom;
                for (int x = xl; x < xr; ++x) {
                    uint16_t val = gen.next();
                    for (int x1 = 0; x1 < zoom; ++x1) {
                        *dst++ = val;
                    }
                }
                for (int y1 = 1; y1 < zoom; ++y1) {
                    dst = (uint16_t *)(pixels + pc_display->pitch * (ybase + y1));
                    memcpy(dst + xl * zoom, dstbase + xl * zoom, zw * zoom * sizeof(uint16_t));
                }
                gen.line(1);
            }
            SDL_UnlockSurface(pc_display);
        }
    }
    void fill(const Rect &rect, RGB565 colour) {
        struct Solid {
            uint16_t val;
            inline uint16_t next() const { return val; }
            inline void line(int) const {}
            inline void skip(int) const {}
        };
        Solid solid{colour.val()};
        output(rect, solid);
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
        output(rect, slurp);
    }
    void copy1bit(const Point &pt, int pixels, const uint8_t *src, int x_offset, int pitch, RGB565 bg, RGB565 fg) {
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
        output(Rect(pt.x, pt.y, pt.x + pixels, pt.y + 8), slurp);
    }
};

void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg)
{
    int ys = std::max<int16_t>(0, yc - r);
    int ye = std::min<int16_t>(disp.height(), yc + r + 1);
    int w = disp.width();

    for (int y = ys; y < ye; ++y) {
        float xrf = sqrtf(r * r - (y - yc) * (y - yc));
        int xr = ceil(xrf);
        disp.fill(Rect(std::max<int16_t>(0, xc - xr), y, std::min<int16_t>(xc + xr + 1, w), y + 1), fg);
    }
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL Init failed\n");
        exit(1);
    }

    SDLDisplay display(480, 320, 2);
    
    bool quit = false;
    display.init();
    int t = 0;
    static const uint8_t face[8] = { 255, 195, 165, 129, 165, 153, 195, 255 };
    RGB565 face2[16 * 16];
    for (int i = 0; i < 16; ++i) {
        int t = 7 + (i < 8 ? i : 16 - i);
        auto fg = RGB565::rgb888(t * 16, 0, 0);
        for (int j = 0; j < 16; ++j) {
            face2[16 * i + j] = face[i >> 1] & (128 >> (j >> 1)) ? RGB565::rgb888(0xFFFFFF) : fg;
        }
    }
    //display.setClipRect(Rect(40, 40, 480 - 40, 320 - 40));
    display.setClipRect(Rect(5, 5, 480 - 5, 320 - 5));
    while(true) {
        t++;
        for (int i = 0; i < 30; ++i) {
            for (int j = 0; j < 20; ++j) {
                RGB565 colour = ((i ^ j) & 1) ? RGB565::rgb888(0xFFFFFF) : RGB565::rgb888(0x000000);
                display.fill(Rect(i * 16, j * 16, (i + 1) * 16, (j + 1) * 16), colour);
            }
        }
        for (int i = 0; i < 30; i += 4) {
            for (int j = 0; j < 20; j += 4) {
                display.copy(Rect(i * 16, j * 16, i * 16 + 16, j * 16 + 16), face2);
            }
        }
        for (int i = 2; i < 30; i += 4) {
            for (int j = 2; j < 20; j += 4) {
                display.copy1bit(Point(i * 16 + 4, j * 16 + 4), 8, face, 0, 1, RGB565::rgb888(0x00FF00), RGB565::rgb888(0x000000));
            }
        }
        circle(display, (t * 3) % (480 + 2 * 120) - 120, 160, 120, RGB565::rgb888(0xFFFF00));
        display.complete();

        usleep(20000);
        SDL_Event sdlEvent;
        bool quit = false;
        while(SDL_PollEvent(&sdlEvent)) {
            if (sdlEvent.type == SDL_QUIT)
                quit = true;
        }
        if (quit)
            break;
    }
    SDL_Quit();
    return 0;
}
