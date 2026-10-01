#include <Arduino.h>
#include <stm32h7xx.h>
#include "display.h"
#include "fonts.h"
#include "mipi.h"
#include "stm32.h"
#include "demo.h"

#define LCD_ADDR_LINE 16
#define LCD_RESET_PIN PA7
#define LCD_BRIGHTNESS_PIN PB14
#define LCD_IS_IPS true

auto &display_fmcdma()
{
    static uint16_t buffer[480 * 10 + 5];
    static ChunkOutputDisplay<MIPIDisplay<STM32FMCWithDMA>, 480, 320> singleton(buffer, std::size(buffer));
    return singleton;
}

auto &display_fmccopy()
{
    static uint16_t buffer[480 * 10 + 5];
    static ChunkOutputDisplay<MIPIDisplay<STM32FMC>, 480, 320> singleton(buffer, std::size(buffer));
    return singleton;
}

auto &display_direct()
{
    static STM32Display<MIPIDisplay<STM32FMC>, 480, 320> singleton;
    return singleton;
}

auto &display()
{
    //return display_direct();
    //return display_fmccopy();
    return display_fmcdma();
}

extern "C" void DMA1_Stream0_IRQHandler(void)
{
    STM32FMCWithDMA::onDMAInterrupt();
}

extern "C" int _write(const void *, int)
{
    return 0;
}

Demo demo;

void setup() {
    STM32FMCWithDMA::initMPU(MPU_REGION_NUMBER0);
    STM32FMCWithDMA::initPins(LCD_ADDR_LINE);
    STM32FMCWithDMA::initFMC();

    __HAL_RCC_DMA1_CLK_ENABLE();
    STM32FMCWithDMA::initDMA(DMA1_Stream0, DMA1_Stream0_IRQn);

    pinMode(LCD_RESET_PIN, OUTPUT);
    digitalWrite(LCD_RESET_PIN, LOW);
    delay(1);
    digitalWrite(LCD_RESET_PIN, HIGH);
    delay(10);

    using MIPI = MIPIDisplay<STM32FMC>;

    auto &d = display();
    // Init the display
    MIPI::setSleep(false);
    delay(20);
    MIPI::configure();
    MIPI::setInvert(LCD_IS_IPS);

    analogWrite(LCD_BRIGHTNESS_PIN, 1023);

    demo.init(d);
}

void loop()
{
    demo.loop(display(), millis());
}
