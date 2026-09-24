# Draws neon versions of Luna the keyboard pet from her original 1-bit OLED sprites in luna_sprites.h.
# Run inside QMK MSYS (it needs Pillow):
#   python3 prepare_luna.py
# then convert each result with QMK's painter tool:
#   for f in luna_*.png; do qmk painter-convert-graphics -f pal256 -i $f; done
import re
from PIL import Image, ImageChops, ImageFilter

from prepare_logos import BG, add_light, gradient, hex_rgb

SCALE = 3  # each sprite pixel becomes 3x3 on screen
PAD = 6    # room for the glow
WORK = 4   # drawn at 4x size, then scaled down so the glow is smooth
SPRITES = ['sit', 'walk', 'run', 'bark', 'sneak']


def sprite(src, name, frame):
    m = re.search(r'\bluna_%s\[2\]\[\w+\] = \{(.*?)\n\};' % name, src, re.S)
    data = [int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', m.group(1))][frame * 96:(frame + 1) * 96]
    img = Image.new('L', (32, 22), 0)
    for y in range(22):
        for x in range(32):
            if (data[(y // 8) * 32 + x] >> (y % 8)) & 1:
                img.putpixel((x, y), 255)
    return img


def neon_luna(img):
    k = SCALE * WORK
    big = img.resize((img.width * k, img.height * k), Image.NEAREST)
    shape = Image.new('L', (big.width + 2 * PAD * WORK, big.height + 2 * PAD * WORK), 0)
    shape.paste(big, (PAD * WORK, PAD * WORK))
    tube = shape.filter(ImageFilter.GaussianBlur(WORK * 0.4))
    halo = shape.filter(ImageFilter.GaussianBlur(WORK * 2.2))
    colour = gradient(shape.size, [(0.0, hex_rgb('#ff4fd8')), (0.5, hex_rgb('#9b3dff')), (1.0, hex_rgb('#2fc2ff'))])
    core = Image.blend(colour, Image.new('RGB', shape.size, (255, 255, 255)), 0.35)
    out = Image.new('RGB', shape.size, BG)
    out = add_light(out, colour, halo, 1.6)
    out = add_light(out, core, tube)
    return out.resize((shape.width // WORK, shape.height // WORK), Image.LANCZOS)


if __name__ == '__main__':
    src = open('luna_sprites.h').read()
    for name in SPRITES:
        for frame in (0, 1):
            neon_luna(sprite(src, name, frame)).save('luna_%s_%d.png' % (name, frame))
