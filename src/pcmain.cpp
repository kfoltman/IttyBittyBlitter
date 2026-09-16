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
public:
    SDLDisplay(int _zoom = 1)
    : BaseDisplay(480, 320)
    , zoom(_zoom)
    {}
    void init() override {
        pc_display = SDL_SetVideoMode(w * zoom, h * zoom, 16, 0);
    }
    void complete() {
        SDL_Flip(pc_display);
    }
    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.width() < 0 || rect.height() < 0)
            return;
        if (!SDL_LockSurface(pc_display)) {
            uint8_t *pixels = (uint8_t *)pc_display->pixels;
            int xs = rect.left() * zoom;
            int xe = rect.right() * zoom;
            for (int y = rect.top(); y < rect.bottom(); ++y) {
                int ybase = y * zoom;
                uint16_t *dstbase = (uint16_t *)(pixels + pc_display->pitch * ybase);
                uint16_t *dst = dstbase + rect.left() * zoom;
                for (int x = rect.left(); x < rect.right(); ++x) {
                    uint16_t val = gen.next();
                    for (int x1 = 0; x1 < zoom; ++x1) {
                        *dst++ = val;
                    }
                }
                for (int y1 = 1; y1 < zoom; ++y1) {
                    dst = (uint16_t *)(pixels + pc_display->pitch * (ybase + y1));
                    memcpy(dst + rect.left() * zoom, dstbase + rect.left() * zoom, rect.width() * zoom * sizeof(uint16_t));
                }
                gen.line();
            }
            SDL_UnlockSurface(pc_display);
        }
    }
    void fill(const Rect &rect, RGB565 colour) {
        struct Solid {
            uint16_t val;
            inline uint16_t next() const { return val; }
            inline void line() const {}
        };
        Solid solid{colour.val()};
        output(rect, solid);
    }
    void copy(const Rect &rect, const RGB565 *src) {
        struct Slurp {
            const RGB565 *src;
            inline uint16_t next() { return (src++)->val(); }
            inline void line() const {}
        };
        Slurp slurp{src};
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
            inline void line() {
                glyphs += pitch;
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

    SDLDisplay display(2);
    
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
