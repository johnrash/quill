#!/usr/bin/env python3
"""Writes Quill's application icon as HVIF (Haiku Vector Icon Format).

HVIF layout, from Haiku's FlatIconImporter: magic "ncif", then styles,
paths and shapes, each list prefixed by a count byte. Coordinates are on a
64 x 64 canvas and stored in one byte (value + 32) or two bytes
((value + 128) * 102 with the high bit set).

Usage: tools/make_icon.py Quill.hvif
"""
import math
import struct
import sys


def coord(value):
    if float(value).is_integer() and -32 <= value <= 95:
        return bytes([int(value) + 32])
    v = int(round((value + 128.0) * 102.0))
    return bytes([(v >> 8) | 0x80, v & 0xFF])


def float24(value):
    if value == 0:
        return b"\x00\x00\x00"
    bits = struct.unpack(">I", struct.pack(">f", value))[0]
    sign = bits >> 31
    exponent = ((bits >> 23) & 0xFF) - 127
    mantissa = (bits & 0x7FFFFF) >> 6
    stored = (sign << 23) | ((exponent + 32) << 17) | mantissa
    return bytes([(stored >> 16) & 0xFF, (stored >> 8) & 0xFF, stored & 0xFF])


def solid(r, g, b, a=255):
    if a == 255:
        return bytes([3, r, g, b])
    return bytes([1, r, g, b, a])


def linear(stops, start, end):
    """Linear gradient from point start to point end (icon coordinates)."""
    (x0, y0), (x1, y1) = start, end
    dx, dy = x1 - x0, y1 - y0
    length = math.hypot(dx, dy)
    s = length / 128.0
    angle = math.atan2(dy, dx)
    cos_a, sin_a = math.cos(angle) * s, math.sin(angle) * s
    # agg::trans_affine: sx, shy, shx, sy, tx, ty
    matrix = [cos_a, sin_a, -sin_a, cos_a, (x0 + x1) / 2, (y0 + y1) / 2]
    alpha = any(len(color) == 4 and color[3] != 255 for _, color in stops)
    flags = 0x02 | (0 if alpha else 0x04)
    out = bytes([2, 0, flags, len(stops)])
    for value in matrix:
        out += float24(value)
    for offset, color in stops:
        out += bytes([int(offset * 255)])
        out += bytes(color[:3])
        if alpha:
            out += bytes([color[3] if len(color) == 4 else 255])
    return out


def polygon(points):
    """A closed path of straight lines."""
    out = bytes([0x02 | 0x08, len(points)])
    for x, y in points:
        out += coord(x) + coord(y)
    return out


def curve_path(points):
    """A closed path; each point is (x, y, in_x, in_y, out_x, out_y)."""
    out = bytes([0x02, len(points)])
    for point in points:
        for value in point:
            out += coord(value)
    return out


def shape(style, paths):
    out = bytes([10, style, len(paths)])
    out += bytes(paths)
    out += bytes([0])  # no flags
    return out


def smooth(points):
    """Catmull-Rom style control points for a closed smooth outline."""
    result = []
    n = len(points)
    for i in range(n):
        x, y = points[i]
        px, py = points[i - 1]
        nx, ny = points[(i + 1) % n]
        tx, ty = (nx - px) / 6.0, (ny - py) / 6.0
        result.append((x, y, x - tx, y - ty, x + tx, y + ty))
    return result


def main():
    styles = []
    paths = []
    shapes = []

    def add_style(data):
        styles.append(data)
        return len(styles) - 1

    def add_path(data):
        paths.append(data)
        return len(paths) - 1

    outline = add_style(solid(40, 40, 40))
    paper = add_style(linear([(0, (255, 255, 255)), (1, (214, 218, 226))],
                             (14, 6), (46, 58)))
    fold = add_style(linear([(0, (250, 250, 250)), (1, (168, 172, 180))],
                            (40, 6), (46, 14)))
    ink = add_style(solid(70, 74, 90))
    heading = add_style(solid(24, 52, 140))
    shadow = add_style(solid(0, 0, 0, 60))
    feather = add_style(linear([(0, (255, 251, 236)), (1, (196, 170, 120))],
                               (54, 6), (30, 46)))
    feather_edge = add_style(solid(120, 88, 40))
    shaft = add_style(solid(110, 76, 30))
    nib = add_style(linear([(0, (60, 60, 70)), (1, (10, 10, 14))],
                           (24, 46), (18, 58)))

    # shadow of the page
    shapes.append(shape(shadow, [add_path(polygon(
        [(14, 9), (44, 9), (52, 17), (52, 61), (14, 61)]))]))
    # page outline and page
    shapes.append(shape(outline, [add_path(polygon(
        [(10, 4), (41, 4), (50, 13), (50, 59), (10, 59)]))]))
    shapes.append(shape(paper, [add_path(polygon(
        [(11, 5), (40.5, 5), (49, 13.5), (49, 58), (11, 58)]))]))
    # folded corner
    shapes.append(shape(outline, [add_path(polygon(
        [(40, 4.5), (49.5, 14), (40, 14)]))]))
    shapes.append(shape(fold, [add_path(polygon(
        [(41, 6), (48, 13), (41, 13)]))]))

    # a heading and lines of text
    shapes.append(shape(heading, [add_path(polygon(
        [(15, 17), (33, 17), (33, 20), (15, 20)]))]))
    for y, right in ((25, 44), (29, 45), (33, 41), (37, 45), (41, 43),
                     (45, 36), (50, 45), (54, 32)):
        shapes.append(shape(ink, [add_path(polygon(
            [(15, y), (right, y), (right, y + 1.6), (15, y + 1.6)]))]))

    # the quill: a feather from the top right down to the nib
    vane = smooth([(58, 3), (60, 9), (55, 20), (46, 32), (36, 42),
                   (28, 47), (26, 45), (30, 37), (38, 25), (47, 13)])
    shapes.append(shape(feather_edge, [add_path(curve_path(
        [(x + (0.8 if x > 40 else 0.4), y + 0.8, ix, iy + 0.8, ox, oy + 0.8)
         for x, y, ix, iy, ox, oy in vane]))]))
    shapes.append(shape(feather, [add_path(curve_path(vane))]))
    # shaft
    shapes.append(shape(shaft, [add_path(polygon(
        [(57.5, 4.5), (58.5, 5.5), (27.5, 46.5), (26.5, 45.5)]))]))
    # barbs
    for t in (0.25, 0.42, 0.58, 0.74):
        x = 58 - t * 31
        y = 5 + t * 41
        shapes.append(shape(feather_edge, [add_path(polygon(
            [(x, y), (x + 5, y + 1.2), (x + 5, y + 2), (x, y + 0.9)]))]))
    # nib
    shapes.append(shape(nib, [add_path(polygon(
        [(26, 44), (29, 47), (19, 59), (17, 60), (18, 57.5)]))]))

    data = b"ncif"
    data += bytes([len(styles)]) + b"".join(styles)
    data += bytes([len(paths)]) + b"".join(paths)
    data += bytes([len(shapes)]) + b"".join(shapes)
    with open(sys.argv[1], "wb") as out:
        out.write(data)
    print("wrote %s: %d bytes, %d styles, %d paths, %d shapes" % (
        sys.argv[1], len(data), len(styles), len(paths), len(shapes)))


if __name__ == "__main__":
    main()
