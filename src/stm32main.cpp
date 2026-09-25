#include <Arduino.h>
#include <stm32h7xx.h>
#include "display.h"
#include "fonts.h"
#include "stm32.h"
#include "demo.h"

#define LCD_ADDR_LINE 16
#define LCD_RESET_PIN PA7
#define LCD_BRIGHTNESS_PIN PB14
#define LCD_IS_IPS true

auto &display() {
    static STM32Display<ILI9488<STM32FMC<LCD_ADDR_LINE, MPU_REGION_NUMBER0>, LCD_RESET_PIN, LCD_BRIGHTNESS_PIN, LCD_IS_IPS>> singleton;
    return singleton;
}

extern "C" int _write(const void *, int)
{
    return 0;
}

Demo demo;

void setup()
{
    auto &d = display();
    d.init();
    demo.init();
    d.fill(Rect(0, 0, d.width(), d.height()), RGB565::rgb888(0x000000));
}

void loop()
{
    demo.loop(display(), millis());
}
