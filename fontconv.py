import sys

from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

def char_range(chFrom, chFromNot, inclusive=False):
    return map(chr, range(chFrom, chFromNot + (1 if inclusive else 0)))

def font_export(of, export_name, family, size, weight=QFont.Weight.Normal, italic=False, charset=None, antialias=False, antialias_bpp=4, extra_chars=""):
    if charset is None:
        charset = char_range(33, 127)
    charset = list(charset) + list(extra_chars)
    charset = list(sorted(charset))
    lowest = ord(min(charset))
    highest = ord(max(charset))
    font = QFont(family, int(size), weight, italic)
    font.setPixelSize(size)
    if not antialias:
        font.setStyleStrategy(QFont.StyleStrategy.NoAntialias)
    else:
        font.setStyleStrategy(QFont.StyleStrategy.PreferAntialias | QFont.StyleStrategy.NoSubpixelAntialias)
    font.setHintingPreference(QFont.HintingPreference.PreferFullHinting)
    font.setFixedPitch(True)
    finfo = QFontInfo(font)
    if font.family() != finfo.family():
        print (f"Warning: Using a substitute font {finfo.family()} instead of {font.family()}")
    metrics = QFontMetrics(font)
    charDefs = []
    totalWidth = 0
    maxHeight = 0
    minWidth = 1000
    maxWidth = 0
    maxColour = 15
    firstChar = None
    charCount = 0
    for ch in charset:
        if firstChar is None:
            firstChar = ord(ch)
        charCount += 1
        size = metrics.size(0, ch)
        #print (size, metrics.height(), metrics.ascent())
        if antialias:
            pixmap = QPixmap(size)
        else:
            pixmap = QBitmap(size)
        pixmap.fill(QColor(0, 0, 0))
        painter = QPainter(pixmap)
        painter.setPen(QPen(QColor(255, 255, 255)))
        painter.setFont(font)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing, antialias)
        painter.drawText(QRectF(0.5, 0.5, 128, 128), Qt.AlignTop | Qt.AlignLeft, ch)
        painter = None
        charDefs.append((ch, size, pixmap))
        totalWidth += size.width()
        minWidth = min(minWidth, size.width())
        maxWidth = max(maxWidth, size.width())
        maxHeight = max(maxHeight, size.height())
    if antialias:
        overallPixmap = QPixmap(totalWidth, maxHeight)
    else:
        overallPixmap = QBitmap(totalWidth, maxHeight)
    overallPixmap.fill(QColor(255, 255, 255))
    painter = QPainter(overallPixmap)
    x = 0
    nbytes = 0
    font_bits = ""
    font_widths = ""
    font_xs = ""
    xsmap = {}
    for ch, size, pixmap in charDefs:
        img = pixmap.toImage()
        # font_bits += f"// Character: '{ch}'\n"
        xsmap[ord(ch)] = x
        font_widths += f"{size.width()}, // '{ch}'\n"
        painter.drawPixmap(x, 0, pixmap)
        x += size.width()
        xsmap[ord(ch) + 1] = x
    last = 0
    for i in range(lowest, highest + 1):
        next = xsmap.get(i, last)
        font_xs += f"    {next}, // '{chr(i)}'\n"
        last = next
    font_xs += f"    {x}, // end\n"
    painter = None
    bpp = antialias_bpp if antialias else 1
    print (f"{x} x {maxHeight}, {minWidth}..{maxWidth}")
    img = overallPixmap.toImage().convertToFormat(QImage.Format_Grayscale8)
    img.save(f"{export_name}.png")
    size = overallPixmap.size()

    for iy in range(maxHeight):
        font_bits += "    "
        if antialias:
            if antialias_bpp == 8:
                for ix in range(0, totalWidth):
                    if ix < size.width() and iy < size.height():
                        byteValue = img.pixelColor(ix, iy).value()
                    else:
                        byteValue = 0
                    font_bits += f"0x{byteValue:02x}, "
                    nbytes += 1
            else:
                for ix in range(0, totalWidth, 2):
                    byteValue = 0
                    for ix2 in range(ix, min(ix + 2, totalWidth)):
                        if ix2 < size.width() and iy < size.height():
                            byteValue |= (img.pixelColor(ix2, iy).value() * maxColour // 255) << (4 - 4 * (ix2 - ix))
                    font_bits += f"0x{byteValue:02x}, "
                    nbytes += 1
        else:
            for ix in range(0, totalWidth, 8):
                byteValue = 0
                for ix2 in range(ix, min(ix + 8, totalWidth)):
                    if ix2 < size.width() and iy < size.height() and img.pixelColor(ix2, iy).value() > 127:
                        byteValue |= 128 >> (ix2 - ix)
                font_bits += f"0x{byteValue:02x}, "
                nbytes += 1
        font_bits = font_bits.rstrip()
        font_bits += "\n"

    of.write(f"""\
static const uint16_t {export_name}_xs[] = {"{"}
{font_xs}{"}"};

static const uint8_t {export_name}_bits[] = {"{"}
{font_bits}{"}"};

Font {export_name} = {"{"}
    .xs={export_name}_xs,
    .bits={export_name}_bits,
    .firstChar={firstChar}, .charCount={highest-lowest+1},
    .height={maxHeight},
    .spaceWidth={metrics.size(0, ' ').width()},
    .pitch={nbytes // maxHeight},
{"}"};
""")
    return nbytes
    
nbytes = 0
app = QGuiApplication(sys.argv)
with open("src/fonts.inc", "w") as of:
    nbytes += font_export(of, 'font_small', 'DejaVu Sans', 22, antialias=True)
    nbytes += font_export(of, 'font_large', 'DejaVu Sans Mono', 48, antialias=True, charset=".:0123456789ABCXYZ")

print (f"Total {nbytes} bytes")
