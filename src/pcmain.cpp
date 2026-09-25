#include <SDL/SDL.h>
#include <ctime>
#include "display.h"
#include "fonts.h"

#include "demo.h"

class SDLDisplay: public BaseDisplayOps<SDLDisplay>
{
private:
    int zoom;
    SDL_Surface *pc_display;
public:
    SDLDisplay(int _w, int _h, int _zoom = 1)
    : BaseDisplayOps(_w, _h)
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
};

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL Init failed\n");
        exit(1);
    }

    SDLDisplay display(480, 320, 2);
    Demo demo;

    bool quit = false;
    display.init();
    demo.init();
    
    clock_t start = clock();

    while(true) {
        demo.loop(display, (clock() - start) / 1000);

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
