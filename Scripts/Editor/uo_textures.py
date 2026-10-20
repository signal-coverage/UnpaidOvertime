"""Procedural texture generation (pure Python PNG writer) + import into /Game/UO/Textures.

Unreal's embedded Python has no PIL, so images are built as bytearrays and encoded with zlib+struct.
Every generator is deterministic (fixed seeds) so re-running produces identical files.
"""
import math
import os
import random
import struct
import zlib

import unreal

import uo_common as C

# --------------------------------------------------------------------------------------------
# PNG + image helpers
# --------------------------------------------------------------------------------------------


def write_png(path, w, h, rgb):
    """rgb: bytearray of w*h*3 bytes."""
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)
        raw.extend(rgb[y * stride:(y + 1) * stride])

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")
    C.ensure_dir(os.path.dirname(path))
    with open(path, "wb") as f:
        f.write(png)


def clamp01(v):
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


def to_bytes(w, h, fn):
    """fn(x, y) -> (r, g, b) floats 0..1 (sRGB)."""
    out = bytearray(w * h * 3)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = fn(x, y)
            out[i] = int(clamp01(r) * 255.0 + 0.5)
            out[i + 1] = int(clamp01(g) * 255.0 + 0.5)
            out[i + 2] = int(clamp01(b) * 255.0 + 0.5)
            i += 3
    return out


class Noise:
    """Tileable value noise on a w x h image with `cx` x `cy` lattice cells."""

    def __init__(self, w, h, cx, cy, seed):
        rnd = random.Random(seed)
        self.cx, self.cy = cx, cy
        self.g = [[rnd.random() for _ in range(cx)] for _ in range(cy)]
        self.sx = cx / float(w)
        self.sy = cy / float(h)

    def at(self, x, y):
        fx = x * self.sx
        fy = y * self.sy
        ix = int(fx)
        iy = int(fy)
        tx = fx - ix
        ty = fy - iy
        tx = tx * tx * (3 - 2 * tx)
        ty = ty * ty * (3 - 2 * ty)
        x0 = ix % self.cx
        x1 = (ix + 1) % self.cx
        y0 = iy % self.cy
        y1 = (iy + 1) % self.cy
        g = self.g
        a = g[y0][x0] + (g[y0][x1] - g[y0][x0]) * tx
        b = g[y1][x0] + (g[y1][x1] - g[y1][x0]) * tx
        return a + (b - a) * ty


def noise_field(w, h, octaves, seed, base_cx=4, base_cy=None, persistence=0.5):
    """Precomputed fbm field (list of floats 0..1), tileable."""
    base_cy = base_cy or base_cx
    layers = [Noise(w, h, base_cx * (2 ** o), base_cy * (2 ** o), seed + o * 101) for o in range(octaves)]
    amps = [persistence ** o for o in range(octaves)]
    total = sum(amps)
    field = [0.0] * (w * h)
    i = 0
    for y in range(h):
        for x in range(w):
            v = 0.0
            for layer, amp in zip(layers, amps):
                v += layer.at(x, y) * amp
            field[i] = v / total
            i += 1
    return field


def white_noise(w, h, seed):
    rnd = random.Random(seed)
    return [rnd.random() for _ in range(w * h)]


def lerp(a, b, t):
    return a + (b - a) * t


def lerp3(c0, c1, t):
    return (lerp(c0[0], c1[0], t), lerp(c0[1], c1[1], t), lerp(c0[2], c1[2], t))


def hexc(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def normal_from_height(w, h, height, strength):
    out = bytearray(w * h * 3)
    i = 0
    for y in range(h):
        ym = ((y - 1) % h) * w
        yp = ((y + 1) % h) * w
        row = y * w
        for x in range(w):
            xm = (x - 1) % w
            xp = (x + 1) % w
            dx = (height[row + xp] - height[row + xm]) * strength
            dy = (height[yp + x] - height[ym + x]) * strength
            nx, ny, nz = -dx, -dy, 1.0
            inv = 1.0 / math.sqrt(nx * nx + ny * ny + nz * nz)
            out[i] = int((nx * inv * 0.5 + 0.5) * 255)
            out[i + 1] = int((ny * inv * 0.5 + 0.5) * 255)
            out[i + 2] = int((nz * inv * 0.5 + 0.5) * 255)
            i += 3
    return out


# --------------------------------------------------------------------------------------------
# Tiny 5x7 bitmap font (block letters for posters, screens and the clock)
# --------------------------------------------------------------------------------------------

FONT = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00010", "00010", "00010", "00010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
    " ": ["00000"] * 7,
    ".": ["00000", "00000", "00000", "00000", "00000", "01100", "01100"],
    ",": ["00000", "00000", "00000", "00000", "01100", "00100", "01000"],
    "!": ["00100", "00100", "00100", "00100", "00100", "00000", "00100"],
    "?": ["01110", "10001", "00001", "00010", "00100", "00000", "00100"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    ":": ["00000", "01100", "01100", "00000", "01100", "01100", "00000"],
    "%": ["11001", "11010", "00010", "00100", "01000", "01011", "10011"],
    "'": ["00100", "00100", "01000", "00000", "00000", "00000", "00000"],
}


class Canvas:
    def __init__(self, w, h, color):
        self.w, self.h = w, h
        self.px = [list(color) for _ in range(w * h)]

    def rect(self, x0, y0, x1, y1, color, alpha=1.0):
        x0, y0 = max(0, int(x0)), max(0, int(y0))
        x1, y1 = min(self.w, int(x1)), min(self.h, int(y1))
        for y in range(y0, y1):
            row = y * self.w
            for x in range(x0, x1):
                p = self.px[row + x]
                if alpha >= 1.0:
                    p[0], p[1], p[2] = color
                else:
                    p[0] = lerp(p[0], color[0], alpha)
                    p[1] = lerp(p[1], color[1], alpha)
                    p[2] = lerp(p[2], color[2], alpha)

    def vgradient(self, x0, y0, x1, y1, c0, c1):
        for y in range(int(y0), int(y1)):
            t = (y - y0) / float(max(1, y1 - y0 - 1))
            self.rect(x0, y, x1, y + 1, lerp3(c0, c1, t))

    def circle(self, cx, cy, r, color):
        for y in range(int(cy - r), int(cy + r) + 1):
            for x in range(int(cx - r), int(cx + r) + 1):
                if 0 <= x < self.w and 0 <= y < self.h:
                    d = math.hypot(x - cx, y - cy)
                    if d <= r:
                        a = clamp01(r - d)
                        p = self.px[y * self.w + x]
                        p[0] = lerp(p[0], color[0], a)
                        p[1] = lerp(p[1], color[1], a)
                        p[2] = lerp(p[2], color[2], a)

    def polygon(self, pts, color):
        ys = [p[1] for p in pts]
        for y in range(max(0, int(min(ys))), min(self.h, int(max(ys)) + 1)):
            xs = []
            n = len(pts)
            for i in range(n):
                xa, ya = pts[i]
                xb, yb = pts[(i + 1) % n]
                if (ya <= y < yb) or (yb <= y < ya):
                    xs.append(xa + (y - ya) * (xb - xa) / (yb - ya))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                self.rect(xs[i], y, xs[i + 1] + 1, y + 1, color)

    def text_width(self, text, scale, spacing=1):
        return len(text) * (5 + spacing) * scale - spacing * scale

    def text(self, x, y, text, scale, color, spacing=1):
        cx = x
        for ch in text.upper():
            glyph = FONT.get(ch, FONT[" "])
            for gy, row in enumerate(glyph):
                for gx, bit in enumerate(row):
                    if bit == "1":
                        self.rect(cx + gx * scale, y + gy * scale, cx + (gx + 1) * scale, y + (gy + 1) * scale, color)
            cx += (5 + spacing) * scale

    def text_centered(self, cy, text, scale, color, spacing=1):
        w = self.text_width(text, scale, spacing)
        self.text((self.w - w) // 2, cy, text, scale, color, spacing)

    def to_bytes(self, out_h=None):
        """Optionally resample vertically (nearest) to out_h rows."""
        out_h = out_h or self.h
        out = bytearray(self.w * out_h * 3)
        i = 0
        for y in range(out_h):
            sy = min(self.h - 1, int(y * self.h / float(out_h)))
            row = sy * self.w
            for x in range(self.w):
                p = self.px[row + x]
                out[i] = int(clamp01(p[0]) * 255 + 0.5)
                out[i + 1] = int(clamp01(p[1]) * 255 + 0.5)
                out[i + 2] = int(clamp01(p[2]) * 255 + 0.5)
                i += 3
        return out


# --------------------------------------------------------------------------------------------
# Generators. Each returns list of (name, w, h, bytes, kind) where kind is 'color' or 'normal'.
# Texel scale comments describe the real-world size one texture repeat covers (UVs are 1 unit = 1 m).
# --------------------------------------------------------------------------------------------


def gen_carpet():
    """512 px = 100 cm = 2x2 carpet tiles of 50 cm, quarter-turned loop pattern."""
    w = h = 512
    n = noise_field(w, h, 3, 11, 8)
    fine = white_noise(w, h, 12)
    base = hexc("847a6f")
    dark = hexc("5d554d")
    light = hexc("a59a8b")
    height = [0.0] * (w * h)

    def px(x, y):
        tx, ty = x // 256, y // 256
        lx, ly = x % 256, y % 256
        horizontal = (tx + ty) % 2 == 0
        coord = ly if horizontal else lx
        across = lx if horizontal else ly
        loop = 0.5 + 0.5 * math.sin(coord * 2 * math.pi / 5.0 + 0.6 * math.sin(across * 0.21))
        band = 0.5 + 0.5 * math.sin(across * 2 * math.pi / 64.0)
        i = y * w + x
        v = 0.55 * loop + 0.25 * fine[i] + 0.2 * n[i]
        seam = 1.0 if (lx < 2 or ly < 2) else 0.0
        height[i] = v * 0.7 - seam * 0.8
        c = lerp3(dark, base, clamp01(v * 1.2))
        c = lerp3(c, light, clamp01((fine[i] - 0.82) * 4.0))
        c = lerp3(c, dark, 0.18 * band)
        if seam:
            c = lerp3(c, dark, 0.55)
        return c

    color = to_bytes(w, h, px)
    normal = normal_from_height(w, h, height, 1.4)
    return [("T_Carpet_D", w, h, color, "color"), ("T_Carpet_N", w, h, normal, "normal")]


def gen_ceiling_tile():
    """512 px = 60 cm acoustic tile with exposed T-bar grid on the border."""
    w = h = 512
    n = noise_field(w, h, 3, 21, 8)
    fine = white_noise(w, h, 22)
    rnd = random.Random(23)
    holes = set()
    for _ in range(2600):
        x, y = rnd.randrange(w), rnd.randrange(h)
        length = rnd.randrange(1, 5)
        dx, dy = rnd.choice([(1, 0), (0, 1), (1, 1), (1, -1)])
        for k in range(length):
            holes.add(((x + dx * k) % w, (y + dy * k) % h))
    tile = hexc("e9e6df")
    hole_c = hexc("b9b4aa")
    tbar = hexc("f4f4f2")
    shadow = hexc("a8a49c")
    height = [0.0] * (w * h)

    def px(x, y):
        i = y * w + x
        d = min(x, y, w - 1 - x, h - 1 - y)
        if d < 6:
            height[i] = 1.0
            return tbar
        if d < 9:
            height[i] = 0.3
            return lerp3(shadow, tile, (d - 6) / 3.0)
        v = 0.92 + 0.08 * n[i] + 0.03 * fine[i]
        c = (tile[0] * v, tile[1] * v, tile[2] * v)
        if (x, y) in holes:
            height[i] = 0.0
            return hole_c
        height[i] = 0.5 + 0.05 * fine[i]
        return c

    color = to_bytes(w, h, px)
    normal = normal_from_height(w, h, height, 2.5)
    return [("T_CeilingTile_D", w, h, color, "color"), ("T_CeilingTile_N", w, h, normal, "normal")]


def gen_wood():
    """512 px = 100 cm light wood (birch/maple laminate), grain along U."""
    w = h = 512
    warp = noise_field(w, h, 3, 31, 2, 6)
    fine = noise_field(w, h, 2, 32, 64, 4)
    light = hexc("d9b98f")
    mid = hexc("c49e72")
    dark = hexc("a9825a")

    def px(x, y):
        i = y * w + x
        g = math.sin((y + warp[i] * 70.0) * 2 * math.pi / 23.0)
        g2 = math.sin((y + warp[i] * 40.0) * 2 * math.pi / 7.3)
        t = 0.5 + 0.35 * g + 0.15 * g2
        c = lerp3(light, mid, clamp01(t))
        c = lerp3(c, dark, clamp01((fine[i] - 0.55) * 1.6) * 0.5)
        return c

    return [("T_Wood_D", w, h, to_bytes(w, h, px), "color")]


def gen_brushed_metal():
    """512 px = 50 cm brushed metal streaks (neutral, tinted by material)."""
    w = h = 512
    streak = noise_field(w, h, 3, 41, 2, 96)
    fine = white_noise(w, h, 42)

    def px(x, y):
        i = y * w + x
        v = 0.78 + 0.14 * streak[i] + 0.05 * fine[i]
        return (v, v, v)

    return [("T_BrushedMetal_D", w, h, to_bytes(w, h, px), "color")]


def gen_fabric():
    """512 px = 25 cm woven fabric (neutral gray, tinted per material)."""
    w = h = 512
    n = noise_field(w, h, 3, 51, 8)
    fine = white_noise(w, h, 52)
    height = [0.0] * (w * h)

    def px(x, y):
        i = y * w + x
        period = 8
        cx, cy = (x // period) % 2, (y // period) % 2
        fx = (x % period) / float(period)
        fy = (y % period) / float(period)
        warp = math.sin(fx * math.pi)
        weft = math.sin(fy * math.pi)
        top = warp if (cx ^ cy) else weft
        heather = 0.85 + 0.15 * fine[i]
        v = (0.62 + 0.25 * top) * heather * (0.92 + 0.12 * n[i])
        height[i] = top
        return (v, v, v * 1.02)

    color = to_bytes(w, h, px)
    normal = normal_from_height(w, h, height, 1.2)
    return [("T_Fabric_D", w, h, color, "color"), ("T_Fabric_N", w, h, normal, "normal")]


def gen_kitchen_tile():
    """512 px = 60 cm = 2x2 floor tiles of 30 cm with grout."""
    w = h = 512
    n = noise_field(w, h, 3, 61, 6)
    fine = white_noise(w, h, 62)
    rnd = random.Random(63)
    tints = [0.95 + 0.06 * rnd.random() for _ in range(4)]
    tile = hexc("dcd8ce")
    grout = hexc("9e9a92")
    height = [0.0] * (w * h)

    def px(x, y):
        i = y * w + x
        lx, ly = x % 256, y % 256
        d = min(lx, ly, 255 - lx, 255 - ly)
        if d < 3:
            height[i] = 0.0
            return grout
        bevel = clamp01((d - 3) / 3.0)
        height[i] = bevel
        t = tints[(x // 256) + 2 * (y // 256)]
        v = t * (0.94 + 0.06 * n[i] + 0.02 * fine[i])
        c = (tile[0] * v, tile[1] * v, tile[2] * v)
        return lerp3(grout, c, 0.6 + 0.4 * bevel)

    color = to_bytes(w, h, px)
    normal = normal_from_height(w, h, height, 3.0)
    return [("T_KitchenTile_D", w, h, color, "color"), ("T_KitchenTile_N", w, h, normal, "normal")]


def gen_wall_paint():
    """512 px = 200 cm subtle orange-peel paint (neutral, tinted per material)."""
    w = h = 512
    n = noise_field(w, h, 4, 71, 4)
    fine = white_noise(w, h, 72)

    def px(x, y):
        i = y * w + x
        v = 0.9 + 0.06 * n[i] + 0.025 * fine[i]
        return (v, v, v)

    return [("T_WallPaint_D", w, h, to_bytes(w, h, px), "color")]


def gen_asphalt():
    """512 px = 400 cm asphalt with aggregate speckle."""
    w = h = 512
    n = noise_field(w, h, 4, 81, 4)
    fine = white_noise(w, h, 82)
    base = hexc("4a4a4c")

    def px(x, y):
        i = y * w + x
        v = 0.8 + 0.3 * n[i] + 0.25 * (fine[i] - 0.5)
        if fine[i] > 0.985:
            v += 0.35
        return (base[0] * v, base[1] * v, base[2] * v * 1.02)

    return [("T_Asphalt_D", w, h, to_bytes(w, h, px), "color")]


def gen_concrete():
    """512 px = 300 cm sidewalk concrete with expansion joints every 150 cm."""
    w = h = 512
    n = noise_field(w, h, 4, 91, 4)
    fine = white_noise(w, h, 92)
    base = hexc("b5b1a8")

    def px(x, y):
        i = y * w + x
        v = 0.88 + 0.12 * n[i] + 0.06 * (fine[i] - 0.5)
        if x % 256 < 2 or y % 256 < 2:
            v *= 0.6
        return (base[0] * v, base[1] * v, base[2] * v)

    return [("T_Concrete_D", w, h, to_bytes(w, h, px), "color")]


def gen_grass():
    w = h = 512
    n = noise_field(w, h, 4, 101, 4)
    fine = white_noise(w, h, 102)
    a = hexc("5f7f3a")
    b = hexc("86a04e")

    def px(x, y):
        i = y * w + x
        return lerp3(a, b, clamp01(n[i] * 0.8 + fine[i] * 0.4))

    return [("T_Grass_D", w, h, to_bytes(w, h, px), "color")]


def gen_leaves():
    w = h = 512
    n = noise_field(w, h, 3, 111, 8)
    fine = white_noise(w, h, 112)
    a = hexc("3f6b2f")
    b = hexc("78a24a")

    def px(x, y):
        i = y * w + x
        blob = 0.5 + 0.5 * math.sin(x * 0.19 + n[i] * 9.0) * math.sin(y * 0.17 + n[i] * 7.0)
        return lerp3(a, b, clamp01(blob * 0.6 + fine[i] * 0.3 + n[i] * 0.3))

    return [("T_Leaves_D", w, h, to_bytes(w, h, px), "color")]


def gen_blinds():
    """256 px = 50 cm of horizontal blind slats (slat pitch 2.5 cm)."""
    w = h = 256
    fine = white_noise(w, h, 121)

    def px(x, y):
        i = y * w + x
        t = (y % 13) / 13.0
        shade = 0.72 + 0.28 * math.sin(t * math.pi)
        if y % 13 == 12:
            shade = 0.45
        v = shade * (0.97 + 0.03 * fine[i])
        return (v * 0.95, v * 0.95, v * 0.93)

    return [("T_Blinds_D", w, h, to_bytes(w, h, px), "color")]


def gen_facade():
    """512 px = 1200 cm square of a distant office facade (4 floors x 6 bays)."""
    w = h = 512
    n = noise_field(w, h, 3, 131, 6)
    wall = hexc("c9c2b6")
    band = hexc("8d8a84")
    glass_top = hexc("6f8ea6")
    glass_bot = hexc("2f4252")
    rnd = random.Random(132)
    lit = {(bx, by): rnd.random() for bx in range(6) for by in range(4)}

    def px(x, y):
        i = y * w + x
        bx, lx = x // 85, x % 85
        by, ly = y // 128, y % 128
        v = 0.92 + 0.08 * n[i]
        if ly < 14:
            return (band[0] * v, band[1] * v, band[2] * v)
        if 10 < lx < 78 and 30 < ly < 116:
            t = (ly - 30) / 86.0
            c = lerp3(glass_top, glass_bot, t)
            if lit.get((bx % 6, by % 4), 0) > 0.75:
                c = lerp3(c, hexc("e8e2c8"), 0.35)
            if lx in (44, 45):
                c = band
            return c
        return (wall[0] * v, wall[1] * v, wall[2] * v)

    return [("T_Facade_D", w, h, to_bytes(w, h, px), "color")]


def gen_screen():
    """512x256 monitor wallpaper: a passive-aggressive spreadsheet."""
    cv = Canvas(512, 256, hexc("e9eef3"))
    cv.rect(0, 0, 512, 22, hexc("2c5a85"))
    cv.text(8, 4, "Q3 TPS REPORT FINAL V7", 2, hexc("ffffff"))
    cv.rect(0, 22, 512, 36, hexc("d5dde6"))
    for col in range(9):
        cv.rect(col * 58, 36, col * 58 + 1, 236, hexc("b8c3cf"))
    rnd = random.Random(141)
    for row in range(13):
        y = 40 + row * 15
        cv.rect(0, y + 14, 512, y + 15, hexc("c8d1db"))
        for col in range(9):
            if rnd.random() < 0.7:
                ln = rnd.randrange(14, 46)
                cv.rect(col * 58 + 5, y + 5, col * 58 + 5 + ln, y + 9, hexc("4a5866"))
    cv.rect(300, 120, 500, 200, hexc("ffffff"))
    cv.rect(300, 120, 500, 134, hexc("b3262e"))
    cv.text(306, 123, "OVERDUE", 1, hexc("ffffff"))
    for k, hgt in enumerate([30, 46, 22, 58, 40]):
        cv.rect(312 + k * 36, 194 - hgt, 334 + k * 36, 194, hexc("2c5a85") if k != 3 else hexc("b3262e"))
    cv.rect(0, 236, 512, 256, hexc("1f2a36"))
    cv.rect(6, 240, 20, 252, hexc("5aa05a"))
    return [("T_Screen_D", 512, 256, cv.to_bytes(), "color")]


def gen_clock():
    w = h = 256
    cv = Canvas(w, h, hexc("f7f5ef"))
    cx = cy = 128
    for k in range(60):
        a = k / 60.0 * 2 * math.pi
        r0 = 108 if k % 5 else 96
        for t in range(r0, 116):
            x = cx + math.sin(a) * t
            y = cy - math.cos(a) * t
            s = 2 if k % 5 else 3
            cv.rect(x - s / 2, y - s / 2, x + s / 2 + 1, y + s / 2 + 1, hexc("2a2a2a"))
    for num, ang in (("12", 0), ("3", 90), ("6", 180), ("9", 270)):
        a = math.radians(ang)
        x = cx + math.sin(a) * 76
        y = cy - math.cos(a) * 76
        sw = cv.text_width(num, 3)
        cv.text(x - sw / 2, y - 10, num, 3, hexc("2a2a2a"))
    cv.text_centered(160, "ACME", 1, hexc("b3262e"))
    return [("T_ClockFace_D", w, h, cv.to_bytes(), "color")]


POSTERS = [
    ("T_Poster_Synergy", "SYNERGY", ["TOGETHER WE ACHIEVE", "MORE UNPAID HOURS"], "sunrise", hexc("141414"), hexc("ffffff")),
    ("T_Poster_Overtime", "OVERTIME", ["IS AN OPPORTUNITY", "NOT A CHOICE"], "mountain", hexc("8f1d22"), hexc("ffe9e0")),
    ("T_Poster_Teamwork", "TEAMWORK", ["MAKES THE DREAM WORK", "THE WORK IS ALSO YOURS."], "rowers", hexc("16324f"), hexc("ffffff")),
    ("T_Poster_Smile", "SMILE", ["YOUR PERFORMANCE", "REVIEW IS WATCHING"], "eye", hexc("2f3b1f"), hexc("d9f27a")),
    ("T_Poster_Trust", "TRUST", ["EVERYONE IS ON YOUR TEAM.", "PROBABLY."], "handshake", hexc("1b1b1b"), hexc("ffffff")),
]


def draw_poster(title, lines, art, bg, fg):
    """Classic motivational-poster layout on a 512x768 canvas (resampled to 512x1024 on export)."""
    w, h = 512, 768
    cv = Canvas(w, h, bg)
    ax0, ay0, ax1, ay1 = 56, 56, 456, 456
    cv.rect(ax0 - 4, ay0 - 4, ax1 + 4, ay1 + 4, fg)
    if art == "sunrise":
        cv.vgradient(ax0, ay0, ax1, ay1, hexc("f6c26b"), hexc("e0603a"))
        cv.circle(256, 330, 70, hexc("fff3c2"))
        cv.polygon([(ax0, 360), (180, 260), (300, 380), (380, 300), (ax1, 360), (ax1, ay1), (ax0, ay1)], hexc("3c2a3a"))
    elif art == "mountain":
        cv.vgradient(ax0, ay0, ax1, ay1, hexc("9cc6e0"), hexc("e6eef2"))
        cv.polygon([(ax0, 420), (210, 170), (290, 280), (340, 220), (ax1, 420), (ax1, ay1), (ax0, ay1)], hexc("4a5866"))
        cv.polygon([(210, 170), (245, 220), (225, 230), (195, 205)], hexc("ffffff"))
        cv.rect(206, 140, 210, 172, hexc("2a2a2a"))
        cv.polygon([(210, 140), (236, 148), (210, 156)], hexc("b3262e"))
    elif art == "rowers":
        cv.vgradient(ax0, ay0, ax1, ay1, hexc("f2d8a7"), hexc("7aa3c4"))
        cv.rect(ax0, 330, ax1, ay1, hexc("3b6f96"))
        cv.polygon([(110, 322), (400, 322), (380, 342), (130, 342)], hexc("2a2a2a"))
        for k in range(6):
            x = 150 + k * 40
            cv.circle(x, 286, 9, hexc("2a2a2a"))
            cv.rect(x - 6, 294, x + 6, 324, hexc("2a2a2a"))
            cv.polygon([(x + 4, 310), (x + 7, 306), (x - 30, 360), (x - 34, 356)], hexc("2a2a2a"))
    elif art == "eye":
        cv.vgradient(ax0, ay0, ax1, ay1, hexc("1c2412"), hexc("3a4a22"))
        cv.polygon([(96, 256), (256, 156), (416, 256), (256, 356)], hexc("e8f5b0"))
        cv.circle(256, 256, 70, hexc("6f8f2a"))
        cv.circle(256, 256, 34, hexc("0e120a"))
        cv.circle(236, 236, 12, hexc("ffffff"))
    else:  # handshake
        cv.vgradient(ax0, ay0, ax1, ay1, hexc("dfe6ea"), hexc("aab8c2"))
        cv.polygon([(80, 300), (230, 250), (270, 280), (120, 340)], hexc("c58f6a"))
        cv.polygon([(432, 300), (282, 250), (242, 280), (392, 340)], hexc("8f6248"))
        cv.rect(60, 290, 100, 360, hexc("2c3e50"))
        cv.rect(412, 290, 452, 360, hexc("2c3e50"))
        cv.polygon([(300, 262), (340, 240), (346, 252), (306, 274)], hexc("b3262e"))
    scale = 10 if len(title) <= 7 else 8
    cv.text_centered(500, title, scale, fg, spacing=2)
    y = 600
    for ln in lines:
        s = 3 if len(ln) <= 24 else 2
        cv.text_centered(y, ln, s, fg)
        y += 9 * s + 10
    return cv.to_bytes(1024)


def gen_posters():
    out = []
    for name, title, lines, art, bg, fg in POSTERS:
        out.append((name, 512, 1024, draw_poster(title, lines, art, bg, fg), "color"))
    return out


def gen_sign():
    """Door plaques: men / women restroom pictograms and the server room warning (256x256 each)."""
    out = []
    cv = Canvas(256, 256, hexc("2e5b8a"))
    cv.circle(128, 62, 24, hexc("ffffff"))
    cv.polygon([(96, 96), (160, 96), (150, 170), (106, 170)], hexc("ffffff"))
    cv.rect(108, 170, 124, 226, hexc("ffffff"))
    cv.rect(132, 170, 148, 226, hexc("ffffff"))
    out.append(("T_Sign_Men_D", 256, 256, cv.to_bytes(), "color"))
    cv = Canvas(256, 256, hexc("8a2e5b"))
    cv.circle(128, 62, 24, hexc("ffffff"))
    cv.polygon([(128, 92), (172, 182), (84, 182)], hexc("ffffff"))
    cv.rect(110, 182, 122, 226, hexc("ffffff"))
    cv.rect(134, 182, 146, 226, hexc("ffffff"))
    out.append(("T_Sign_Women_D", 256, 256, cv.to_bytes(), "color"))
    cv = Canvas(256, 256, hexc("8f1d22"))
    cv.rect(8, 8, 248, 248, hexc("a8252c"))
    cv.text_centered(70, "SERVER", 6, hexc("ffffff"))
    cv.text_centered(140, "AUTHORIZED", 3, hexc("ffd0c8"))
    cv.text_centered(176, "ONLY", 3, hexc("ffd0c8"))
    out.append(("T_Sign_Server_D", 256, 256, cv.to_bytes(), "color"))
    return out


GENERATORS = [gen_carpet, gen_ceiling_tile, gen_wood, gen_brushed_metal, gen_fabric, gen_kitchen_tile,
              gen_wall_paint, gen_asphalt, gen_concrete, gen_grass, gen_leaves, gen_blinds, gen_facade,
              gen_screen, gen_clock, gen_posters, gen_sign]


# --------------------------------------------------------------------------------------------
# Import
# --------------------------------------------------------------------------------------------


def import_texture(png_path, name, kind):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png_path)
    task.set_editor_property("destination_path", C.TEX_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    C.asset_tools().import_asset_tasks([task])
    tex = C.load(C.TEX_DIR + "/" + name)
    if tex is None:
        C.warn("Texture import failed: " + name)
        return None
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    C.save(tex)
    return tex


def build(regenerate_png=True):
    C.ensure_dir(C.TEX_SOURCE_DIR)
    count = 0
    for gen in GENERATORS:
        for name, w, h, data, kind in gen():
            path = os.path.join(C.TEX_SOURCE_DIR, name + ".png")
            if regenerate_png or not os.path.exists(path):
                write_png(path, w, h, data)
            import_texture(path, name, kind)
            count += 1
            C.log("texture %s (%dx%d %s)" % (name, w, h, kind))
    C.log("textures done: %d" % count)
