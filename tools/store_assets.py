"""Draws the store icons (144x144, 48x48) and the banner (720x320) into store/assets/.
Same motif as resources/images/menu_icon.png: calendar with red header, dark grid, one red day.
Run: python3 tools/store_assets.py   (needs Pillow; the banner uses store/screenshots/emery-1-day.png)"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter

RED = (220, 30, 30)
DARK = (25, 25, 25)
WHITE = (255, 255, 255)
COBALT = (0, 85, 170)
FONT_BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def calendar(size):
    """Calendar motif on a transparent background, drawn 4x larger and scaled down."""
    s = size * 4
    im = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = s / 24.0  # grid unit
    m = 2 * u     # margin
    top = 4 * u
    r = 2.2 * u
    # body with border
    d.rounded_rectangle([m, top, s - m, s - m], radius=r, fill=WHITE, outline=DARK, width=int(1.3 * u))
    # red header
    d.rounded_rectangle([m, top, s - m, top + 5 * u], radius=r, fill=RED, outline=DARK, width=int(1.3 * u))
    d.rectangle([m + 1.3 * u, top + 3 * u, s - m - 1.3 * u, top + 5 * u - 1.3 * u + 1], fill=RED)
    d.line([m, top + 5 * u, s - m, top + 5 * u], fill=DARK, width=int(1.3 * u))
    # rings
    for x in (7 * u, 17 * u):
        d.rounded_rectangle([x - 0.9 * u, 1.6 * u, x + 0.9 * u, top + 2.2 * u], radius=0.6 * u, fill=DARK)
    # day grid 4x3
    cell = 2.2 * u
    xs = [5.2 * u + i * 4.0 * u for i in range(4)]
    ys = [11.6 * u + j * 3.6 * u for j in range(3)]
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            d.rounded_rectangle([x, y, x + cell, y + cell], radius=0.4 * u, fill=RED if (i, j) == (2, 1) else DARK)
    return im.resize((size, size), Image.LANCZOS)


def banner():
    w, h = 720, 320
    im = Image.new('RGB', (w, h), COBALT)
    d = ImageDraw.Draw(im)
    # soft diagonal gradient
    for y in range(h):
        t = y / h
        d.line([(0, y), (w, y)], fill=(0, int(85 - 35 * t), int(170 - 60 * t)))
    # watch with screenshot on the right
    shot = Image.open('store/screenshots/emery-1-day.png').convert('RGB')
    sw, sh = shot.size
    scale = 250.0 / sh
    shot = shot.resize((int(sw * scale), int(sh * scale)), Image.LANCZOS)
    bx = w - shot.width - 70
    by = (h - shot.height) // 2
    pad = 14
    shadow = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle([bx - pad + 6, by - pad + 8, bx + shot.width + pad + 6, by + shot.height + pad + 8],
                                             radius=28, fill=(0, 0, 0, 110))
    shadow = shadow.filter(ImageFilter.GaussianBlur(8))
    im.paste(shadow, (0, 0), shadow)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([bx - pad, by - pad, bx + shot.width + pad, by + shot.height + pad], radius=28, fill=(30, 30, 34))
    im.paste(shot, (bx, by))
    # icon + texts on the left
    icon = calendar(84)
    im.paste(icon, (44, 52), icon)
    title = ImageFont.truetype(FONT_BOLD, 34)
    sub = ImageFont.truetype(FONT, 19)
    d.text((44, 152), 'Calendar for', font=title, fill=WHITE)
    d.text((44, 192), 'Office 365', font=title, fill=WHITE)
    d.text((46, 244), 'Your day on your wrist.', font=sub, fill=(220, 232, 245))
    d.text((46, 270), 'No server. No tracking.', font=sub, fill=(220, 232, 245))
    return im


if __name__ == '__main__':
    calendar(144).save('store/assets/icon-144.png')
    calendar(48).save('store/assets/icon-48.png')
    banner().save('store/assets/banner-720x320.png')
    print('store/assets/: icon-144.png, icon-48.png, banner-720x320.png')
