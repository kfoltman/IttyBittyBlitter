#include "display.h"

void circle(BaseDisplay &disp, int xc, int yc, int r, RGB565 fg)
{
    int ys = std::max<int16_t>(0, yc - r);
    int ye = std::min<int16_t>(disp.height(), yc + r + 1);
    int w = disp.width();

    for (int y = ys; y < ye; ++y) {
        float xrf = sqrtf(r * r - (y - yc) * (y - yc));
        int xr = ceil(xrf);
        disp.fill(Rect(std::max<int16_t>(0, xc - xr), y, std::min<int16_t>(xc + xr + 1, w), y + 1), fg);
    }
}

