#include <algorithm>
#include <functional>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <iostream>
#include <display.h>

class SDLDisplay: public BaseDisplay
{
private:
    int zoom;
    SDL_Surface *pc_display;
public:
    SDLDisplay(int _zoom = 1) : zoom(_zoom) {}
    void init() override {
        pc_display = SDL_SetVideoMode(zoom * 480, zoom * 320, 16, 0);
    }
    void complete() {
        SDL_Flip(pc_display);
    }
    void fill(const Rect &rect, RGB565 colour) {
        if (!SDL_LockSurface(pc_display)) {
            int xs = rect.left() * zoom;
            int xe = rect.right() * zoom;
            for (int y = rect.top(); y < rect.bottom(); ++y) {
                int ybase = y * zoom;
                for (int y1 = 0; y1 < zoom; ++y1) {
                    uint16_t *dst = (uint16_t *)(((uint8_t *)pc_display->pixels) + pc_display->pitch * (ybase + y1));
                    for (int x = xs; x < xe; ++x)
                        dst[x] = colour.val();
                }
            }
            
            SDL_UnlockSurface(pc_display);
        }
    }
    void copy(const Rect &rect, const RGB565 *src) {
    }
    void copy1bit(const Point &pt, int pixels, const uint8_t *src, int x_offset, RGB565 bg, RGB565 fg) {
    }
};

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
    // stressTest();
    for (int t = 0; t < 500; ++t) {
        for (int i = 0; i < 48; ++i)
            for (int j = 0; j < 32; ++j) {
                RGB565 colour = RGB565::rgb888(i * 17 + 8 * t, (i + j) * 10 - 6 * t, j * 4 - 3 * t);
                display.fill(Rect(i * 10, j * 10, (i + 1) * 10, (j + 1) * 10), colour);
            }
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
