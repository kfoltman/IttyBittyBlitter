#include "display.h"
#include "fonts.h"
#include <initializer_list>
#include <Arduino.h>

#define LCD_IS_IPS

static volatile uint16_t *const lcd_ctl = reinterpret_cast<volatile uint16_t *>(0x60000000);
static volatile uint16_t *const lcd_data = reinterpret_cast<volatile uint16_t *>(0x60020000);

template<typename... T>
static inline void cmd(uint8_t cmd, T... data)
{
    *lcd_ctl = cmd;
    for(uint8_t val: {data...})
        *lcd_data = val;
}

static inline void cmd(uint8_t cmd)
{
    *lcd_ctl = cmd;
}

class STM32Display: public BaseDisplayOps<STM32Display>
{
public:
    using BaseDisplayOps<STM32Display>::BaseDisplayOps;

    void init();

    template<typename Gen>
    void output(const Rect &rect, Gen &gen) {
        if (rect.empty())
            return;
        Rect clipped = rect.intersection(clip);
        if (clipped.empty())
            return;
        int xl = clipped.left(), xr = clipped.right();
        int yt = clipped.top(), yb = clipped.bottom();

        auto LO = [](uint16_t val) -> uint8_t { return val & 0xFF; };
        auto HI = [](uint16_t val) -> uint8_t { return val >> 8; };
        cmd(0x2a, HI(xl), LO(xl), HI(xr - 1), LO(xr - 1));
        cmd(0x2b, HI(yt), LO(yt), HI(yb - 1), LO(yb - 1));
        cmd(0x2c);
        if (yt > rect.top())
            gen.line(yt - rect.top());
        for (int y = yt; y < yb; ++y) {
            if (xl > rect.left())
                gen.skip(xl - rect.left());
            for (int x = xl; x < xr; ++x)
                *lcd_data = gen.next();
            gen.line(1);
        }
    }
};

template<int WIDTH, int HEIGHT, int STRIPE_HEIGHT>
class STM32DoubleBufferDisplay: public BaseDisplayOps<STM32DoubleBufferDisplay<WIDTH, HEIGHT, STRIPE_HEIGHT> >
{
    uint16_t stripe[STRIPE_HEIGHT][WIDTH];
    int nstripe = 0;
    Rect stripe_rect;
public:
    static const int nstripes = (HEIGHT + STRIPE_HEIGHT - 1) / STRIPE_HEIGHT;

    STM32DoubleBufferDisplay()
    : BaseDisplayOps<STM32DoubleBufferDisplay>(WIDTH, HEIGHT) {}
    
    void init() {
        STM32Display tmp(WIDTH, HEIGHT);
        tmp.init();
        nstripe = 0;
    }
    void complete() {
        int yt = nstripe * STRIPE_HEIGHT, yb = (nstripe + 1) * STRIPE_HEIGHT;
        int xl = 0, xr = WIDTH;
        auto LO = [](uint16_t val) -> uint8_t { return val & 0xFF; };
        auto HI = [](uint16_t val) -> uint8_t { return val >> 8; };
        cmd(0x2a, HI(xl), LO(xl), HI(xr - 1), LO(xr - 1));
        cmd(0x2b, HI(yt), LO(yt), HI(yb - 1), LO(yb - 1));
        cmd(0x2c);
        int sheight = STRIPE_HEIGHT;
        if ((nstripe + 1) * STRIPE_HEIGHT > HEIGHT)
            sheight = HEIGHT - nstripe * STRIPE_HEIGHT;
        for (int i = 0; i < sheight; ++i)
            for (int j = 0; j < WIDTH; ++j)
                *lcd_data = stripe[i][j];
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

void fsmcPinRange(GPIO_TypeDef *td, int from, int to)
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

#define LCD_RESET_PIN PA7

void STM32Display::init()
{
    setClipRect(Rect(0, 0, 480, 320));
    analogWrite(PB14, 1023);

    // Set up FMC
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

    uint32_t btr = (3 << 20) | (7 << 16) | (7 << 8) | (7 << 4) | (7 << 0);
    *(volatile uint32_t *)0x52004000 = bcr;
    *(volatile uint32_t *)0x52004004 = btr;

    // Reset the display
    pinMode(LCD_RESET_PIN, OUTPUT);
    digitalWrite(LCD_RESET_PIN, LOW);
    delay(20);
    digitalWrite(LCD_RESET_PIN, HIGH);
    delay(20);

    // Init the display
    cmd(0x11);  // Sleep OUT

    delay(20);

    // Configure the display
    cmd(0x36, 0x28); // memory access control
    cmd(0x3A, 0x55);  // Interface pixel format: 16-bit
    cmd(0XB0, 0x00);  // Interface Mode Control
    cmd(0xB4, 0x02); // inversion control
    cmd(0xE9, 0x00);

    delay(20);

    cmd(0x34);  // Tearing OFF
    cmd(0x29);  // Display ON
    cmd(0x38);  // Idle OFF
    cmd(0x13);  // Normal mode ON
#ifdef LCD_IS_IPS
    cmd(0x21);  // Display inversion ON
#endif
}

auto &display() {
    // static STM32Display singleton(480, 320);
    static STM32DoubleBufferDisplay<480, 320, 10> singleton;
    return singleton;
}

int t = 0;
const uint8_t face[8] = { 255, 195, 165, 129, 165, 153, 195, 255 };
RGB565 face2[16 * 16];
const char text[] = "Hello cruel world";
int textw = 0;

void setup()
{
    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_CSI_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_EnableCompensationCell();

    // MPU region for the TFT
    HAL_MPU_Disable();
    MPU_Region_InitTypeDef MPU_InitStruct;
    /* TEX0, C0, B0 = Strongly ordered */
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
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

    display().init();

    pinMode(PC5, OUTPUT);
    digitalWrite(PC5, LOW);
    
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
