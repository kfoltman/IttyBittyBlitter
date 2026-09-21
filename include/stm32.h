#pragma once

#include <cstdint>
#include <initializer_list>
#include <Arduino.h>

template<int DataAddrLine, uint32_t mpu_region>
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
    static volatile uint16_t &lcd_ctl;
    static volatile uint16_t &lcd_data;

    static void init()
    {
        // FMC pins
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();
        // FMC itself
        __HAL_RCC_FMC_CLK_ENABLE();

        // MPU region for the TFT
        HAL_MPU_Disable();
        MPU_Region_InitTypeDef MPU_InitStruct;
        /* TEX0, C0, B0 = Strongly ordered */
        MPU_InitStruct.Number           = mpu_region;
        MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
        MPU_InitStruct.BaseAddress      = 0x60000000;
        MPU_InitStruct.Size             = MPU_REGION_SIZE_256MB;
        MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
        MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
        MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
        MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
        MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
        MPU_InitStruct.SubRegionDisable = 0x00;
        MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
        HAL_MPU_ConfigRegion(&MPU_InitStruct);
        HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

        // Set the GPIO properties for FMC pins
        fsmcPinRange(GPIOD, 0, 1);
        fsmcPinRange(GPIOD, 4, 5);
        fsmcPinRange(GPIOD, 7, 11);
        fsmcPinRange(GPIOD, 14, 15);
        fsmcPinRange(GPIOE, 7, 15);

        // Program the FMC
        uint32_t bcr =  (1 << 31) |
                        (0 << 14) |  // EXTMOD (diffferent timings for R vs W)
                        (1 << 12) |  // write enable
                        (1 << 7) | // reserved
                        (1 << 4) | // 16 bit bus width
                        (0 << 2) | // NOR flash
                        1;

        // Those timings are OK for 480 MHz. They can be reduced substantially
        // for lower clock rates!
        uint32_t btr = (3 << 20) | (7 << 16) | (7 << 8) | (7 << 4) | (7 << 0);
        *(volatile uint32_t *)0x52004000 = bcr;
        *(volatile uint32_t *)0x52004004 = btr;
    }

    static inline void cmd(uint8_t cmd)
    {
        lcd_ctl = cmd;
    }

    template<typename... T>
    static inline void cmd(uint8_t cmd, T... data)
    {
        lcd_ctl = cmd;
        for(uint8_t val: {data...})
            lcd_data = val;
    }

    static inline void pixel(uint16_t data)
    {
        lcd_data = data;
    }
};

template<int DataAddrLine, uint32_t mpu_region>
volatile uint16_t &STM32FMC<DataAddrLine, mpu_region>::lcd_ctl = *reinterpret_cast<volatile uint16_t *>(0x60000000);

template<int DataAddrLine, uint32_t mpu_region>
volatile uint16_t &STM32FMC<DataAddrLine, mpu_region>::lcd_data = *reinterpret_cast<volatile uint16_t *>(0x60000000 + (2 << DataAddrLine));

template<class DisplayBus, int ResetPin, int BrightnessPin, bool IsIPS>
class ILI9488: public DisplayBus
{
    static inline constexpr uint8_t LO(uint16_t val) { return val & 0xFF; };
    static inline constexpr uint8_t HI(uint16_t val) { return val >> 8; };

public:
    static constexpr int WIDTH = 480;
    static constexpr int HEIGHT = 320;

    static inline void writeArea(int xl, int yt, int xr, int yb) {
        DisplayBus::cmd(0x2a, HI(xl), LO(xl), HI(xr - 1), LO(xr - 1));
        DisplayBus::cmd(0x2b, HI(yt), LO(yt), HI(yb - 1), LO(yb - 1));
        DisplayBus::cmd(0x2c);
    }
    
    static void init() {
        DisplayBus::init();
        
        // Reset the display
        pinMode(ResetPin, OUTPUT);
        digitalWrite(ResetPin, LOW);
        delay(20);
        digitalWrite(ResetPin, HIGH);
        delay(20);
        
        // Init the display
        DisplayBus::cmd(0x11);  // Sleep OUT

        delay(20);

        // Configure the display
        DisplayBus::cmd(0x36, 0x28); // memory access control
        DisplayBus::cmd(0x3A, 0x55); // Interface pixel format: 16-bit
#if 0
        // These don't seem to be doing anything useful
        DisplayBus::cmd(0XB0, 0x00); // Interface Mode Control
        DisplayBus::cmd(0xB4, 0x02); // Display Inversion Control
        DisplayBus::cmd(0xE9, 0x00); // Set Image Function (disable 24-bit data bus)
#endif

        delay(20);

        DisplayBus::cmd(0x34);  // Tearing OFF
        DisplayBus::cmd(0x29);  // Display ON
        DisplayBus::cmd(0x38);  // Idle OFF
        DisplayBus::cmd(0x13);  // Normal mode ON
        if (IsIPS)
            DisplayBus::cmd(0x21);  // Display inversion ON

        setBrightness(1023);
    }
    
    static void setBrightness(int value)
    {
        analogWrite(BrightnessPin, 1023);
    }
};

template<class DisplayInterface>
class STM32Display: public BaseDisplayOps<STM32Display<DisplayInterface> >
{
public:
    static constexpr int WIDTH = DisplayInterface::WIDTH;
    static constexpr int HEIGHT = DisplayInterface::HEIGHT;
    static const int nstripes = 1;

    STM32Display()
    : BaseDisplayOps<STM32Display<DisplayInterface>>(WIDTH, HEIGHT) {}

    void init() {
        DisplayInterface::init();
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

template<class DisplayInterface, int STRIPE_HEIGHT>
class STM32DoubleBufferDisplay: public BaseDisplayOps<STM32DoubleBufferDisplay<DisplayInterface, STRIPE_HEIGHT>>
{
    uint16_t stripe[STRIPE_HEIGHT][(DisplayInterface::WIDTH)];
    int nstripe = 0;
    Rect stripe_rect;
public:
    static constexpr int WIDTH = DisplayInterface::WIDTH;
    static constexpr int HEIGHT = DisplayInterface::HEIGHT;
    static const int nstripes = (HEIGHT + STRIPE_HEIGHT - 1) / STRIPE_HEIGHT;

    STM32DoubleBufferDisplay()
    : BaseDisplayOps<STM32DoubleBufferDisplay>(WIDTH, HEIGHT) {}
    
    void init() {
        DisplayInterface::init();
        nstripe = 0;
    }
    void complete() {
        const int yt = nstripe * STRIPE_HEIGHT;
        const int yb = std::min<int>(yt + STRIPE_HEIGHT, HEIGHT);
        DisplayInterface::writeArea(0, yt, WIDTH, yb);
        for (int i = 0; i < yb - yt; ++i)
            for (int j = 0; j < WIDTH; ++j)
                DisplayInterface::pixel(stripe[i][j]);
        nstripe = (nstripe + 1) % nstripes;
    }
    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.empty())
            return;
        int ystripe = nstripe * STRIPE_HEIGHT;
        Rect clipped = rect.intersection(this->clip).intersection(Rect(0, ystripe, WIDTH, ystripe + STRIPE_HEIGHT));
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


