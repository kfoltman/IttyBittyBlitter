#include "display.h"
#include "fonts.h"
#include "stm32.h"

#define LCD_ADDR_LINE 16
#define LCD_RESET_PIN PA7
#define LCD_BRIGHTNESS_PIN PB14
#define LCD_IS_IPS true
#define LCD_BUFFERING 10

auto &display() {
#if LCD_BUFFERING != 0
    static STM32DoubleBufferDisplay<ILI9488<STM32FMC<LCD_ADDR_LINE, MPU_REGION_NUMBER0>, LCD_RESET_PIN, LCD_BRIGHTNESS_PIN, LCD_IS_IPS>, LCD_BUFFERING> singleton;
#else
    static STM32Display<ILI9488<STM32FMC<LCD_ADDR_LINE, MPU_REGION_NUMBER0>, LCD_RESET_PIN, LCD_BRIGHTNESS_PIN, LCD_IS_IPS>> singleton;
#endif
    return singleton;
}

int t = 0;
const uint8_t face[8] = { 255, 195, 165, 129, 165, 153, 195, 255 };
RGB565 face2[16 * 16];
const char text[] = "Hello cruel world";
int textw = 0;

void setup()
{
    display().init();

    for (int i = 0; i < 16; ++i) {
        int t = 7 + (i < 8 ? i : 16 - i);
        auto fg = RGB565::rgb888(t * 16, 0, 0);
        for (int j = 0; j < 16; ++j) {
            face2[16 * i + j] = face[i >> 1] & (128 >> (j >> 1)) ? RGB565::rgb888(0xFFFFFF) : fg;
        }
    }

    // display().setClipRect(Rect(5, 5, 480 - 5, 320 - 5));
    textw = font_small.textWidth(text);
}

void loop()
{
    Palette16 palette(RGB565::rgb888(0xFFFF00), RGB565::rgb888((t & 63) * 0x000400));
    Palette16 palette2(RGB565::rgb888(0x000000), RGB565::rgb888((t & 63) * 0x000400));
    
    for (int c = 0; c < display().nstripes; ++c) {
        for (int i = 0; i < 30; ++i) {
            for (int j = 0; j < 20; ++j) {
                RGB565 colour = ((i ^ j) & 1) ? RGB565::rgb888(0xFFFFFF) : RGB565::rgb888(0x000000);
                display().fill(Rect(i * 16, j * 16, (i + 1) * 16, (j + 1) * 16), colour);
            }
        }
        for (int i = 0; i < 30; i += 4) {
            for (int j = 0; j < 20; j += 4) {
                display().copy(Rect(i * 16, j * 16, i * 16 + 16, j * 16 + 16), face2);
            }
        }
        for (int i = 2; i < 30; i += 4) {
            for (int j = 2; j < 20; j += 4) {
                display().copy1bit(Point(i * 16 + 4, j * 16 + 4), 8, 8, face, 0, 1, RGB565::rgb888(0x00FF00), RGB565::rgb888(0x000000));
            }
        }
        int cx = (t * 3) % (480 + 2 * 120) - 120;
        cx = 240;
        circle(display(), cx, 160, 120, RGB565::rgb888(0xFFFF00));
        font_small.drawText(display(), palette, Point(cx - textw / 2, 160 - font_small.height), text);
        font_small.drawText(display(), palette2, Point(cx - textw / 2, 160), text);
        display().complete();
    }
    t++;
    // delayMicroseconds(20000);
}
