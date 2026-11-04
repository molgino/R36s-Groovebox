# Erzeugt src/font.h: 8x16 Graustufen-Glyphen (ASCII 32..126 + Umlaute/ß/°) aus DejaVu Sans Mono Bold.
import sys
from PIL import Image, ImageDraw, ImageFont
ttf = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
f = ImageFont.truetype(ttf, 13)
W, H = 8, 16
out = ["// generiert von tools/genfont.py aus DejaVu Sans Mono Bold (Bitstream Vera Lizenz)",
       "#pragma once", "#include <stdint.h>", "static const int FONT_W=8, FONT_H=16;",
       "// Glyph 0..94 = ASCII 32..126, danach EXTRA (UTF-8 in text() zugeordnet)",
       "static const int FONT_N=%d;" % (95 + len("ÄÖÜäöüß°")), "static const uint8_t FONT_DATA[FONT_N][128] = {"]
for c in list(range(32, 127)) + [ord(x) for x in "ÄÖÜäöüß°"]:
    im = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(im)
    d.text((0, 0), chr(c), font=f, fill=255)
    px = list(im.getdata())
    out.append("{" + ",".join(str(p) for p in px) + "},")
out.append("};")
open(sys.argv[2] if len(sys.argv) > 2 else "src/font.h", "w").write("\n".join(out) + "\n")
