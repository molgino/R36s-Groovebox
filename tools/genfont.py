# Erzeugt src/font.h: 8x16 Graustufen-Glyphen (ASCII 32..126) aus DejaVu Sans Mono Bold.
import sys
from PIL import Image, ImageDraw, ImageFont
ttf = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
f = ImageFont.truetype(ttf, 13)
W, H = 8, 16
out = ["// generiert von tools/genfont.py aus DejaVu Sans Mono Bold (Bitstream Vera Lizenz)",
       "#pragma once", "#include <cstdint>", "static const int FONT_W=8, FONT_H=16;",
       "static const uint8_t FONT_DATA[95][128] = {"]
for c in range(32, 127):
    im = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(im)
    d.text((0, 0), chr(c), font=f, fill=255)
    px = list(im.getdata())
    out.append("{" + ",".join(str(p) for p in px) + "},")
out.append("};")
open(sys.argv[2] if len(sys.argv) > 2 else "src/font.h", "w").write("\n".join(out) + "\n")
