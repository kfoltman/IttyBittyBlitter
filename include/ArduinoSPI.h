#pragma once

#include <SPI.h>

template<class SPIWrapper>
class FourPinSPI
{
public:
    static inline void cmd(uint8_t cmd)
    {
        SPIWrapper::ctl();
        SPIWrapper::transfer(cmd);
        SPIWrapper::data();
    }

    template<typename... T>
    static inline void cmd(uint8_t cmd, T... data)
    {
        SPIWrapper::ctl();
        SPIWrapper::transfer(cmd);
        SPIWrapper::data();
        for(uint8_t val: {data...})
            SPIWrapper::transfer(val);
    }

    static inline void pixel(uint16_t data)
    {
        SPIWrapper::transfer(data & 0xFF);
        SPIWrapper::transfer(data >> 8);
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

