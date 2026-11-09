#include <SDL/SDL.h>
#include <cassert>
#include <ctime>
#include <functional>
#include <semaphore>
#include <thread>
#include "IttyBittyBlitter/display.h"
#include "IttyBittyBlitter/fonts.h"

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
        Rect clipped = rect.intersection(clip);
        if (clipped.empty())
            return;
        int yt = clipped.top(), yb = clipped.bottom();
        int xl = clipped.left(), xr = clipped.right();
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

class SDLChunkSender
{
private:
    static const int zoom{2};
    static SDL_Surface *pc_display;
    static int xl, xr;
    static int yt, yb;
    static int x, y;
private:
    static void copy_bits(const volatile uint16_t *src, uint32_t count) {
        // printf("copy %p %d\n", src, count);
        if (!SDL_LockSurface(pc_display)) {
            uint8_t *pixels = (uint8_t *)pc_display->pixels;
            for (uint32_t i = 0; i < count; ++i) {
                uint16_t colour = *src++;
                uint8_t *base = pixels + pc_display->pitch * y * zoom + 2 * x * zoom;
                for (int y1 = 0; y1 < zoom; ++y1) {
                    uint16_t *rbase = reinterpret_cast<uint16_t *>(base);
                    for (int x1 = 0; x1 < zoom; ++x1) {
                        rbase[x1] = colour;
                    }
                    base += pc_display->pitch;
                }
                if (++x >= xr) {
                    x = xl;
                    if (++y >= yb) {
                        y = yt;
                    }
                }
            }
            SDL_UnlockSurface(pc_display);
        }
    }
    static void copy_bits_thread(const volatile uint16_t **src, uint32_t *count, std::function<void()> *endCallback, std::binary_semaphore *semaphore) {
        while(true) {
            semaphore->acquire();
            if (!*src)
                break;
            copy_bits(*src, *count);
            *src = nullptr;
            *count = 0;
            (*endCallback)();
        }
    }
public:
    static void init(int w, int h) {
        pc_display = SDL_SetVideoMode(w * zoom, h * zoom, 16, 0);
    }
    static void complete() {
        SDL_Flip(pc_display);
    }
    static void idle() {
    }
    static void writeArea(int _xl, int _yt, int _xr, int _yb) {
        x = xl = _xl;
        y = yt = _yt;
        xr = _xr;
        yb = _yb;
    }
    static void pixels(const volatile uint16_t *src, uint32_t count, std::function<void()> endCallback) {
        assert(count || !src);
        static const volatile uint16_t *copier_src;
        static uint32_t copier_count;
        static std::function<void()> copier_endCallback;
        static std::binary_semaphore copier_semaphore{0};
        static std::jthread copier(copy_bits_thread, &copier_src, &copier_count, &copier_endCallback, &copier_semaphore);

        assert(!copier_src);
        assert(!copier_count);
        copier_src = src;
        copier_count = count;
        copier_endCallback = endCallback;
        copier_semaphore.release();
    }
};

SDL_Surface *SDLChunkSender::pc_display;
int SDLChunkSender::x;
int SDLChunkSender::y;
int SDLChunkSender::xl;
int SDLChunkSender::xr;
int SDLChunkSender::yt;
int SDLChunkSender::yb;

using SDLChunkDisplay = ChunkOutputDisplay<SDLChunkSender>;

uint32_t ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000U + ts.tv_nsec / 1000000U;
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL Init failed\n");
        exit(1);
    }

    static uint16_t data[480 * 10 + 20];
    SDLChunkDisplay display(data, std::size(data), 480, 320);
    Demo demo;

    bool quit = false;
    display.init();
    demo.init(display);
    
    clock_t start = ms();

    while(true) {
        uint32_t millis_start = ms();
        demo.loop(display, millis_start - start);
        SDLChunkSender::complete();
        uint32_t millis_end = ms();
        uint32_t elapsed = millis_end - millis_start;
        if (elapsed < 8) {
            struct timespec duration = {0, 1000000LL * (8 - elapsed)};
            nanosleep(&duration, nullptr);
        }

        SDL_Event sdlEvent;
        bool quit = false;
        while(SDL_PollEvent(&sdlEvent)) {
            if (sdlEvent.type == SDL_QUIT)
                quit = true;
        }
        if (quit)
            break;
    }
    // Signal the copier thread to quit
    SDLChunkSender::complete();
    SDLChunkSender::pixels(nullptr, 0, []{});
    SDL_Quit();
    return 0;
}
