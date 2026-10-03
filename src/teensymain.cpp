#include <Arduino.h>
#include "display.h"
#include "fonts.h"
#include "mipi.h"
#include "ArduinoSPI.h"
#include "demo.h"

#define CTL_DATA_PIN 9
#define DISPLAY_CS_PIN 10
#define LCD_IS_IPS true

class SPIWrapper
{
public:
    static void transfer(uint8_t data) {
        SPI.transfer(data);
    }
    static void ctl() {
        digitalWrite(CTL_DATA_PIN, LOW);
    }
    static void data() {
        digitalWrite(CTL_DATA_PIN, HIGH);
    }
};

using MIPI = MIPIDisplay<FourPinSPI<SPIWrapper>>;

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
    SPI.begin();
    pinMode(CTL_DATA_PIN, OUTPUT);
    pinMode(CTL_DATA_PIN, LOW);
    pinMode(DISPLAY_CS_PIN, OUTPUT);
    pinMode(DISPLAY_CS_PIN, LOW);
    
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    
    auto &d = display();
    MIPI::softReset();
    delay(10);
    // Init the display
    MIPI::setSleep(false);
    delay(20);
    MIPI::configure();
    MIPI::setInvert(LCD_IS_IPS);

    demo.init(d);
}

void loop()
{
    demo.loop(display(), millis());
}
