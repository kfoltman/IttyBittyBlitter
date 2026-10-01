#pragma once

#include "display.h"
#include <cstdint>
#include <initializer_list>
#include <functional>
#include "stm32h7xx.h"

class STM32FMC
{
    static void fsmcPinRange(GPIO_TypeDef *td, int from, int to)
    {
        GPIO_InitTypeDef pinInit = {
          .Pin = (1U << (to + 1)) - (1U << from),
          .Mode = GPIO_MODE_AF_PP,
          .Pull = GPIO_NOPULL,
          .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
          .Alternate = GPIO_AF12_FMC
        };
        HAL_GPIO_Init(td, &pinInit);
    }
public:
    static volatile uint16_t *lcd_ctl;
    static volatile uint16_t *lcd_data;

    static void initMPU(uint32_t mpu_region);
    static void initPins(int dataAddrLine);
    static void initFMC();

    static inline void cmd(uint8_t cmd)
    {
        *lcd_ctl = cmd;
    }

    template<typename... T>
    static inline void cmd(uint8_t cmd, T... data)
    {
        *lcd_ctl = cmd;
        for(uint8_t val: {data...})
            *lcd_data = val;
    }

    static inline void pixel(uint16_t data)
    {
        *lcd_data = data;
    }

    static void pixels(const volatile uint16_t *src, uint32_t count, std::function<void()> endCallback)
    {
        for (uint32_t i = 0; i < count; ++i)
            pixel(src[i]);
        endCallback();
    }

    static inline bool busy() {
        return false;
    }
    static void idle() {
        // Should never get called
    }
};

class STM32FMCWithDMA: public STM32FMC
{
public:
    static DMA_HandleTypeDef dma;
    static std::function<void()> dmaCallback;

    static bool initDMA(DMA_Stream_TypeDef *stream, IRQn_Type irq);
    static void onDMAInterrupt();

    static void pixels(const volatile uint16_t *src, uint32_t count, std::function<void()> endCallback);

    static inline bool busy() {
        return (bool)dmaCallback;
    }
    static void idle() {
        __WFI();
    }
};

template<class DisplayInterface, int Width, int Height>
class STM32Display: public BaseDisplayOps<STM32Display<DisplayInterface, Width, Height> >
{
public:
    static const int nstripes = 1;

    STM32Display()
    : BaseDisplayOps<STM32Display<DisplayInterface, Width, Height>>(Width, Height) {}

    void init() {
    }

    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.empty())
            return;
        Rect clipped = rect.intersection(this->clip);
        if (clipped.empty())
            return;
        int xl = clipped.left(), xr = clipped.right();
        int yt = clipped.top(), yb = clipped.bottom();

        DisplayInterface::writeArea(xl, yt, xr, yb);
        if (yt > rect.top())
            gen.line(yt - rect.top());
        for (int y = yt; y < yb; ++y) {
            if (xl > rect.left())
                gen.skip(xl - rect.left());
            for (int x = xl; x < xr; ++x)
                DisplayInterface::pixel(gen.next());
            gen.line(1);
        }
    }
};

template<class DisplayInterface, int Width, int Height, int StripeHeight>
class STM32DoubleBufferDisplay: public BaseDisplayOps<STM32DoubleBufferDisplay<DisplayInterface, Width, Height, StripeHeight>>
{
    uint16_t stripe[StripeHeight][Width];
    int nstripe = 0;
    Rect stripe_rect;
public:
    static const int nstripes = (Height + StripeHeight - 1) / StripeHeight;

    STM32DoubleBufferDisplay()
    : BaseDisplayOps<STM32DoubleBufferDisplay>(Width, Height) {}
    
    void init() {
        nstripe = 0;
    }
    void complete() {
        const int yt = nstripe * StripeHeight;
        const int yb = std::min<int>(yt + StripeHeight, Height);
        DisplayInterface::writeArea(0, yt, Width, yb);
        for (int i = 0; i < yb - yt; ++i)
            for (int j = 0; j < Width; ++j)
                DisplayInterface::pixel(stripe[i][j]);
        nstripe = (nstripe + 1) % nstripes;
    }
    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.empty())
            return;
        int ystripe = nstripe * StripeHeight;
        Rect clipped = rect.intersection(this->clip).intersection(Rect(0, ystripe, Width, ystripe + StripeHeight));
        if (clipped.empty())
            return;
        int xl = clipped.left(), xr = clipped.right();
        int yt = clipped.top(), yb = clipped.bottom();
        if (yt > rect.top())
            gen.line(yt - rect.top());
        for (int y = yt; y < yb; ++y) {
            if (xl > rect.left())
                gen.skip(xl - rect.left());
            for (int x = xl; x < xr; ++x)
                stripe[y - ystripe][x] = gen.next();
            gen.line(1);
        }
    }
};


