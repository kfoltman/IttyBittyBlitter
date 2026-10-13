#pragma once

#include <SPI.h>

template<class SPIWrapper>
class FourPinSPI
{
public:
    static inline void cmd(uint8_t cmd)
    {
        SPIWrapper::start();
        SPIWrapper::ctl();
        SPIWrapper::transfer(cmd);
        SPIWrapper::end();
        SPIWrapper::data();
    }

    template<typename... T>
    static inline void cmd(uint8_t cmd, T... data)
    {
        SPIWrapper::start();
        SPIWrapper::ctl();
        SPIWrapper::transfer(cmd);
        SPIWrapper::end();
        SPIWrapper::start();
        SPIWrapper::data();
        for(uint8_t val: {data...})
            SPIWrapper::transfer(val);
        SPIWrapper::end();
    }

    static inline void pixel(uint16_t data)
    {
        SPIWrapper::transfer(data >> 8);
        SPIWrapper::transfer(data & 0xFF);
    }

    static void pixels(const volatile uint16_t *src, uint32_t count, std::function<void()> endCallback)
    {
        SPIWrapper::start();
        for (uint32_t i = 0; i < count; ++i)
            pixel(src[i]);
        SPIWrapper::end();
        endCallback();
    }

    static inline bool busy() {
        return false;
    }
    static void idle() {
        // Should never get called
    }
};

template<class SPIWrapper>
class AsyncFourPinSPI
{
public:
    static inline void cmd(uint8_t cmd)
    {
        SPIWrapper::start();
        SPIWrapper::ctl();
        SPIWrapper::transfer(cmd);
        SPIWrapper::end();
        SPIWrapper::data();
    }

    template<typename... T>
    static inline void cmd(uint8_t cmd, T... data)
    {
        SPIWrapper::start();
        SPIWrapper::ctl();
        SPIWrapper::transfer(cmd);
        SPIWrapper::end();
        SPIWrapper::start();
        SPIWrapper::data();
        for(uint8_t val: {data...})
            SPIWrapper::transfer(val);
        SPIWrapper::end();
    }

    static inline void pixel(uint16_t data)
    {
        SPIWrapper::transfer(data >> 8);
        SPIWrapper::transfer(data & 0xFF);
    }

    static void pixels(const volatile uint16_t *src, uint32_t count, std::function<void()> endCallback)
    {
        SPIWrapper::bulk(src, count, endCallback);
    }

    static inline bool busy() {
        return SPIWrapper::busy();
    }
    static void idle() {
        // Should never get called
    }
};

