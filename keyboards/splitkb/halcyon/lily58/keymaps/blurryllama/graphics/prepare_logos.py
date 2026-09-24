# Turns full-size Apple, Windows and Tux logo images into the small PNGs used on the status display.
# All three are redrawn in a neon style from the shapes in the source images.
# Run inside QMK MSYS (it needs Pillow):
#   python3 prepare_logos.py <apple image> <windows image> <tux image>
# then convert the results with QMK's painter tool:
#   qmk painter-convert-graphics -f rgb565 -i logo_apple.png
#   qmk painter-convert-graphics -f rgb565 -i logo_windows.png
#   qmk painter-convert-graphics -f rgb565 -i logo_tux.png
import sys
from PIL import Image, ImageChops, ImageFilter

HEIGHT = 78  # logo height, fits between the layer bar and the modifier boxes
BG = (13, 2, 33)  # the status display background, #0d0221
WORK = 4  # the neon effect is drawn at 4x size, then scaled down


def fit(img):
    w, h = img.size
    return img.resize((round(w * HEIGHT / h), HEIGHT), Image.LANCZOS)


def hex_rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def gradient(size, stops):
    # Diagonal gradient from top left (t=0) to bottom right (t=1) through the given (t, colour) stops
    w, h = size
    data = []
    for y in range(h):
        for x in range(w):
            t = 0.55 * x / (w - 1) + 0.45 * y / (h - 1)
            for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
                if t <= t1:
                    f = (t - t0) / (t1 - t0)
                    data.append(tuple(round(a + (b - a) * f) for a, b in zip(c0, c1)))
                    break
    img = Image.new('RGB', size)
    img.putdata(data)
    return img


def add_light(out, colour, alpha, gain=1.0):
    # Additive blend: colour scaled by alpha, like light from a neon tube
    alpha = alpha.point(lambda v: min(255, int(v * gain)))
    return ImageChops.add(out, ImageChops.multiply(colour, Image.merge('RGB', (alpha, alpha, alpha))))


PAD = 36  # room for the glow around the logo, at 4x size


def enlarge(img):
    # Scale a source image to 4x the final logo height, with room around it for the glow
    w, h = img.size
    big_h = HEIGHT * WORK
    img = img.resize((round(w * big_h / h), big_h), Image.LANCZOS)
    out = Image.new(img.mode, (img.width + 2 * PAD, img.height + 2 * PAD), 0)
    out.paste(img, (PAD, PAD))
    return out


def shrink(img):
    return img.resize((img.width // WORK, img.height // WORK), Image.LANCZOS)


def draw_neon(out, shape, stops, line=1.5, glow=2.5, fill=0.0, fill_colour=BG):
    """Adds the outline of an enlarged white-on-black shape mask to `out` as a glowing neon tube."""
    inner = shape.filter(ImageFilter.MinFilter(int(line * WORK) * 2 + 1))
    tube = ImageChops.subtract(shape, inner).filter(ImageFilter.GaussianBlur(WORK * 0.4))
    halo = tube.filter(ImageFilter.GaussianBlur(glow * WORK))
    inner_glow = ImageChops.multiply(tube.filter(ImageFilter.GaussianBlur(glow * WORK * 2)), inner)

    colour = gradient(shape.size, stops)
    core = Image.blend(colour, Image.new('RGB', shape.size, (255, 255, 255)), 0.45)  # hot white-ish core

    if fill:
        out = Image.composite(Image.new('RGB', shape.size, fill_colour), out, inner.point(lambda v: int(v * fill)))
    out = add_light(out, colour, inner_glow, 0.9)
    out = add_light(out, colour, halo, 1.4)
    return add_light(out, core, tube)


def neon(mask, stops, **kwargs):
    shape = enlarge(mask)
    return shrink(draw_neon(Image.new('RGB', shape.size, BG), shape, stops, **kwargs))


def apple(path):
    # White logo on black: a hollow neon outline shading from pink through violet to cyan
    img = Image.open(path).convert('L')
    mask = img.point(lambda v: 255 if v > 128 else 0)
    mask = mask.crop(mask.getbbox())
    neon(mask, [(0.0, hex_rgb('#ff4fd8')), (0.5, hex_rgb('#9b3dff')), (1.0, hex_rgb('#2fc2ff'))]).save('logo_apple.png')


def windows(path):
    # Colourful logo on a baked-in grey/white "transparent" checkerboard: the strongly
    # coloured pixels are the panes. Draw them as glowing electric-blue glass.
    img = Image.open(path).convert('RGB')
    mask = img.convert('HSV').split()[1].point(lambda s: 255 if s > 90 else 0)
    mask = mask.crop(mask.getbbox())
    neon(mask, [(0.0, hex_rgb('#5fd4ff')), (1.0, hex_rgb('#1f4dff'))], fill=0.35, fill_colour=hex_rgb('#0a2a8f')).save('logo_windows.png')


def tux(path):
    # Tux on a transparent background, as a neon sign: a cyan-to-violet outline around a dark body,
    # a soft pink glow for his belly and eyes, and glowing yellow beak and feet
    img = Image.open(path).convert('RGBA')
    img = enlarge(img.crop(img.getchannel('A').getbbox()))
    r, g, b, a = img.split()
    threshold = lambda band, test: band.point(lambda v: 255 if test(v) else 0)
    body = threshold(a, lambda v: v > 128)
    white = ImageChops.multiply(threshold(ImageChops.darker(r, ImageChops.darker(g, b)), lambda v: v > 190), body)
    yellow = ImageChops.multiply(ImageChops.multiply(threshold(r, lambda v: v > 180), threshold(b, lambda v: v < 110)),
                                 ImageChops.multiply(threshold(g, lambda v: v > 100), body))

    out = Image.new('RGB', img.size, BG)
    out = draw_neon(out, body, [(0.0, hex_rgb('#2fc2ff')), (1.0, hex_rgb('#9b3dff'))], fill=0.6, fill_colour=hex_rgb('#170a3d'))
    out = draw_neon(out, white, [(0.0, hex_rgb('#fe75fe')), (1.0, hex_rgb('#ff4fd8'))], line=0.7, glow=1.2, fill=0.4, fill_colour=hex_rgb('#fe75fe'))
    out = draw_neon(out, yellow, [(0.0, hex_rgb('#ffe24a')), (1.0, hex_rgb('#ff9a1a'))], line=1.2, glow=2, fill=0.5, fill_colour=hex_rgb('#ff9a1a'))
    shrink(out).save('logo_tux.png')


if __name__ == '__main__':
    apple(sys.argv[1])
    windows(sys.argv[2])
    tux(sys.argv[3])
