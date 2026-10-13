#pragma once

#include <cstdint>

template<class DisplayBus>
class MIPIDisplay: public DisplayBus
{
    static inline constexpr uint8_t LO(uint16_t val) { return val & 0xFF; };
    static inline constexpr uint8_t HI(uint16_t val) { return val >> 8; };

public:
    using DisplayBus::cmd;

    static inline void writeArea(int xl, int yt, int xr, int yb) {
        cmd(0x2a, HI(xl), LO(xl), HI(xr - 1), LO(xr - 1));
        cmd(0x2b, HI(yt), LO(yt), HI(yb - 1), LO(yb - 1));
        cmd(0x2c);
    }

    static void configure() {
        // Configure the display
        cmd(0x36, 0x28); // memory access control
        cmd(0x3A, 0x55); // Interface pixel format: 16-bit

        setTearing(false);      // Tearing OFF
        setDisplay(true);       // Display ON
        setIdle(false);         // Idle OFF
        setPartial(false);      // Normal mode ON
    }

    static void softReset() {
        cmd(0x01);
    }

    static void setInvert(bool invert) {
        cmd(invert ? 0x21 : 0x20);
    }

    static void setSleep(bool sleep) {
        cmd(sleep ? 0x10 : 0x11);
    }

    static void setIdle(bool idle) {
        cmd(idle ? 0x39 : 0x38);
    }

    static void setDisplay(bool on) {
        cmd(on ? 0x29 : 0x28);
    }
    
    static void setPartial(bool partial) {
        cmd(partial ? 0x12 : 0x13);
    }
    
    static void setTearing(bool tearing, bool hblanks=false) {
        if (tearing)
            cmd(0x35, hblanks);
        else
            cmd(0x34);
    }
};

