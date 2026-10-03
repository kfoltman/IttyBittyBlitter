#include <Arduino.h>
#include "display.h"
#include "fonts.h"
#include "mipi.h"
#include "ArduinoSPI.h"
#include "demo.h"

#define CTL_DATA_PIN 9
#define DISPLAY_CS_PIN 10
#define LCD_IS_IPS false

static SPISettings settings(10000000, MSBFIRST, SPI_MODE0);
static EventResponder responder;
static std::function<void()> spiCallback;

class SPIWrapper
{
public:
    static void init() {
        responder.attachImmediate(SPIWrapper::spiComplete);
    }
    static void start() {
        SPI.beginTransaction(settings);
    }
    static void end() {
        SPI.endTransaction();
    }
    static void transfer(uint8_t data) {
        SPI.transfer(data);
    }
    static void bulk(const volatile uint16_t *data, uint32_t count, std::function<void()> endCallback) {
        start();
        uint16_t *vdata = (uint16_t *)data;
        for (uint32_t i = 0; i < count; i ++)
            vdata[i] = (vdata[i] >> 8) | (vdata[i] << 8);
        spiCallback = endCallback;
        SPI.transfer(vdata, vdata, 2 * count, responder);
    }
    static void spiComplete(EventResponder& responder) {
        end();
        std::function<void()> callback;
        std::swap(callback, spiCallback);
        callback();
    }
    static bool busy() {
        return spiCallback != nullptr;
    }
    static void ctl() {
        digitalWrite(CTL_DATA_PIN, LOW);
    }
    static void data() {
        digitalWrite(CTL_DATA_PIN, HIGH);
    }
};

using MIPI = MIPIDisplay<AsyncFourPinSPI<SPIWrapper>>;

auto &display()
{
    static uint16_t buffer[480 * 10 + 5];
    static ChunkOutputDisplay<MIPI, 320, 240> singleton(buffer, std::size(buffer));
    return singleton;
}

extern "C" int _write(const void *, int)
{
    return 0;
}

Demo demo;

void setup() {
    Serial.begin(115200);
    pinMode(CTL_DATA_PIN, OUTPUT);
    digitalWrite(CTL_DATA_PIN, LOW);
    pinMode(DISPLAY_CS_PIN, OUTPUT);
    digitalWrite(DISPLAY_CS_PIN, LOW);
    SPI.begin();
    SPIWrapper::init();
    
    auto &d = display();
    MIPI::softReset();
    delay(125);

#if 0
    // Init the display. This is shamelessly copied/adapted from Adafruit_ILI9341,
    // and not much of it seems to be required, besides the usual MIPI::configure().
    // Perhaps some old displays didn't have the required configuration in OTP ROM?
    MIPI::cmd(0xEF, 0x03, 0x80, 0x02);
    MIPI::cmd(0xCF, 0x00, 0xC1, 0x30);
    MIPI::cmd(0xED, 0x64, 0x03, 0x12, 0x81);
    MIPI::cmd(0xE8, 0x85, 0x00, 0x78);
    MIPI::cmd(0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02);
    MIPI::cmd(0xF7, 0x20);
    MIPI::cmd(0xEA, 0x00, 0x00);

    MIPI::cmd(0xC0, 0x23); // PWCTR1
    MIPI::cmd(0xC1, 0x10); // PWCTR2
    MIPI::cmd(0xC5, 0x3e, 0x28); // VMCTR1 = 4.050, -1.5
    MIPI::cmd(0xC7, 0x86); // VMCTR2 -56

    MIPI::cmd(0x36, 0x28); // memory access control
    MIPI::cmd(0x37, 0x00); // vertical scroll
    MIPI::cmd(0x3A, 0x55); // Interface pixel format: 16-bit

    MIPI::cmd(0xB1, 0x00, 0x18); // FRMCTR1 79 Hz
    MIPI::cmd(0xB6, 0x08, 0x82, 0x27); // DFUNCTR
    MIPI::cmd(0xF2, 0x00); // 3GAMMA
    MIPI::cmd(0x26, 0x01); // GAMMASET

    MIPI::cmd(0xE0, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00);
    MIPI::cmd(0xE1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F);
#endif

    MIPI::configure();

    MIPI::setSleep(false);
    delay(150);
    MIPI::setDisplay(true);
    delay(150);
    MIPI::setInvert(LCD_IS_IPS);
    delay(20);

    demo.init(d);
}

void loop()
{
    demo.loop(display(), millis());
}
