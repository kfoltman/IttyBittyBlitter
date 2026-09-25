#include "demo.h"
#include <cstdio>

void Demo::init()
{
    palette = Palette16{RGB565::rgb888(0x000000), RGB565::rgb888(0x00FF00)};
}

void Demo::loop(BaseDisplay &display, uint32_t millis)
{
    static const char axis[] = "XYZABC";
    char buf[64];
    t++;
    int width = 300, height = 50;
    for (int i = 0; axis[i]; ++i) {
        snprintf(buf, 64, "%c:%8.3f", axis[i], t * 0.001 + i * 0.25);
        font_large.drawPaddedText(display, palette, Rect(0, i * height, width, (i + 1) * height), Font::DT_RIGHT, buf);
    }
    if (millis) {
        snprintf(buf, 64, "%0.1f FPS", t * 1000.0 / millis);
        Palette16 palette = Palette16{RGB565::rgb888(0xF00000), RGB565::rgb888(0x00FF00)};
        font_small.drawPaddedText(display, palette, Rect(display.width() - 144, 280, display.width(), 320), Font::DT_CENTRE, buf);
    }
    display.complete();
}
