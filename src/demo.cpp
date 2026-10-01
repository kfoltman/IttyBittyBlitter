#include "demo.h"
#include <cstdio>

void Demo::init(BaseDisplay &display)
{
    palette = Palette16{RGB565::rgb888(0x000000), RGB565::rgb888(0x00FF00)};
    display.fill(Rect(0, 0, display.width(), display.height()), RGB565::rgb888(0x000000));
    display.complete();
}

void Demo::loop(BaseDisplay &display, uint32_t millis)
{
    static uint16_t backbuf[32 * 32];
    static BufferDisplay bd(backbuf, 32, 32);
    if (!t) {
        for (int i = 0; i < 32; i += 8)
            for (int j = 0; j < 32; j += 8)
                bd.fill(Rect(i, j, i + 8, j + 8), ((i + j) & 8) ? RGB565::rgb888(0xC0C0C0) : RGB565::rgb888(0x404040));
    }
    display.copy16bit(Point(display.width() - 32, 0), 32, 32, backbuf, 32);

    static const char axis[] = "XYZABC";
    char buf[64];
    int width = 300, height = 50;
    for (int i = 0; axis[i]; ++i) {
        snprintf(buf, 64, "%c:%8.3f", axis[i], t * 0.001 + i * 0.25);
        font_large.drawPaddedText(display, palette, Rect(0, i * height, width, (i + 1) * height), Font::DT_RIGHT, buf);
    }
    if (t) {
        snprintf(buf, 64, "%0.1f FPS", t * 1000.0 / std::max<uint32_t>(1, millis - startTime));
        Palette16 palette = Palette16{RGB565::rgb888(0xF00000), RGB565::rgb888(0x00FF00)};
        font_small.drawPaddedText(display, palette, Rect(display.width() - 144, 280, display.width(), 320), Font::DT_CENTRE, buf);
    } else {
        startTime = millis;
    }
    display.complete();
    t++;
}
