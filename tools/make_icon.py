#!/usr/bin/env python3
# Copyright 2026, Andrea Bernardi
# SPDX-License-Identifier: MIT
#
# make_icon.py [--preview DIR]
#
# Writes the vector icons of SalaGiochi (a smiling sun peeking over a
# gamepad, as on the top bar of the launcher) and of Add games (the same,
# with a green "+"), into SalaGiochi.rdef and installer/AddGames.rdef.
# With --preview, also draws them as PNG files (needs PIL) to look at.

import math
import re
import sys


# #pragma mark - HVIF

def _coord(v):
	"""The two-byte form: value * 102 + 128 * 102, top bit set."""
	value = int(round((v + 128.0) * 102.0))
	if not 0 <= value <= 0x7fff:
		raise ValueError("coordinate %r out of range" % v)
	return bytes([0x80 | (value >> 8), value & 0xff])


def build_hvif(styles, paths, shapes):
	"""styles: (r, g, b, a) colors. paths: closed polygons. shapes: (style,
	[paths]) drawn in order."""
	out = bytearray(b"ncif")
	out.append(len(styles))
	for r, g, b, a in styles:
		out.append(0x01)					# solid color, RGBA
		out += bytes([r, g, b, a])
	out.append(len(paths))
	for points in paths:
		out.append(0x02 | 0x08)				# closed, no curves
		out.append(len(points))
		for x, y in points:
			out += _coord(x) + _coord(y)
	out.append(len(shapes))
	for style, indices in shapes:
		out.append(0x0a)					# path source
		out.append(style)
		out.append(len(indices))
		out += bytes(indices)
		out.append(0x00)					# no transform, no flags
	return bytes(out)


def rdef_block(data):
	lines = ["resource vector_icon {"]
	text = "".join("%02X" % b for b in data)
	for i in range(0, len(text), 64):
		lines.append('\t$"%s"' % text[i:i + 64])
	lines.append("};")
	return "\n".join(lines) + "\n"


# #pragma mark - shapes

def circle(cx, cy, r, steps=32):
	return [(cx + r * math.cos(2 * math.pi * i / steps),
		cy + r * math.sin(2 * math.pi * i / steps)) for i in range(steps)]


def rounded_rect(left, top, right, bottom, r, steps=8):
	points = []
	corners = [(right - r, top + r, -90), (right - r, bottom - r, 0),
		(left + r, bottom - r, 90), (left + r, top + r, 180)]
	for cx, cy, start in corners:
		for i in range(steps + 1):
			angle = math.radians(start + 90 * i / steps)
			points.append((cx + r * math.cos(angle), cy + r * math.sin(angle)))
	return points


def arc_band(cx, cy, r, width, start, end, steps=12):
	"""A thick arc, for the smile."""
	outer = [(cx + (r + width / 2) * math.cos(math.radians(start + (end - start)
		* i / steps)), cy + (r + width / 2) * math.sin(math.radians(start
		+ (end - start) * i / steps))) for i in range(steps + 1)]
	inner = [(cx + (r - width / 2) * math.cos(math.radians(start + (end - start)
		* i / steps)), cy + (r - width / 2) * math.sin(math.radians(start
		+ (end - start) * i / steps))) for i in range(steps + 1)]
	return outer + inner[::-1]


def grown(points, cx, cy, amount):
	"""Pushed out from (cx, cy): a rim around a round shape."""
	result = []
	for x, y in points:
		dx, dy = x - cx, y - cy
		length = math.hypot(dx, dy) or 1
		result.append((x + dx / length * amount, y + dy / length * amount))
	return result


SUN = (255, 201, 60, 255)
SUN_RIM = (226, 144, 30, 255)
NIGHT = (23, 48, 79, 255)
OUTLINE = (14, 30, 51, 255)
BODY = (46, 92, 153, 255)
BODY_LIGHT = (66, 120, 190, 255)
GREEN = (63, 185, 80, 255)
GREEN_DARK = (34, 120, 48, 255)
ORANGE = (255, 138, 91, 255)
WHITE = (255, 255, 255, 255)
SHINE = (255, 255, 255, 110)


def build(plus):
	styles = []
	paths = []
	shapes = []

	def add(color, polygons):
		if color not in styles:
			styles.append(color)
		indices = []
		for polygon in polygons:
			indices.append(len(paths))
			paths.append(polygon)
		shapes.append((styles.index(color), indices))

	# the sun, behind the gamepad, with its rays
	rays = []
	for i in range(8):
		angle = math.radians(-180 + i * 180 / 7)
		x, y = math.cos(angle), math.sin(angle)
		px, py = -y, x
		base, tip = 17.5, 23.5
		rays.append([(32 + x * base + px * 2.6, 22 + y * base + py * 2.6),
			(32 + x * tip, 22 + y * tip),
			(32 + x * base - px * 2.6, 22 + y * base - py * 2.6)])
	add(SUN, rays)
	add(SUN_RIM, [circle(32, 22, 16.5)])
	add(SUN, [circle(32, 22, 15)])
	add(NIGHT, [circle(26.5, 17.5, 2.2, 16), circle(37.5, 17.5, 2.2, 16)])
	add(NIGHT, [arc_band(32, 19, 7, 2.4, 25, 155)])

	# the gamepad: a wide body and two grips, with a dark rim
	body = rounded_rect(5, 29, 59, 47, 9)
	grips = [circle(15, 47, 9.5), circle(49, 47, 9.5)]
	add(OUTLINE, [rounded_rect(3.2, 27.2, 60.8, 48.8, 10.8),
		grown(grips[0], 15, 47, 1.8), grown(grips[1], 49, 47, 1.8)])
	add(BODY, [body] + grips)
	add(BODY_LIGHT, [rounded_rect(8, 30.5, 56, 37, 5)])

	# the cross, and the buttons
	add(OUTLINE, [rounded_rect(14.6, 31.6, 21.4, 46.4, 1.6),
		rounded_rect(10.6, 35.6, 25.4, 42.4, 1.6)])
	add(SUN, [rounded_rect(15.6, 32.6, 20.4, 45.4, 1.2),
		rounded_rect(11.6, 36.6, 24.4, 41.4, 1.2)])
	add(OUTLINE, [circle(44.5, 36.5, 4.6, 20), circle(51, 42, 4.6, 20)])
	add(GREEN, [circle(44.5, 36.5, 3.6, 20)])
	add(ORANGE, [circle(51, 42, 3.6, 20)])
	add(SHINE, [circle(43.4, 35.3, 1.2, 12), circle(49.9, 40.8, 1.2, 12)])

	if plus:
		# Add games: a green "+" in a badge, bottom right
		add(WHITE, [circle(50, 50, 12.5)])
		add(GREEN_DARK, [circle(50, 50, 11)])
		add(GREEN, [circle(50, 49.2, 9.8)])
		add(WHITE, [rounded_rect(48, 42.5, 52, 56, 1),
			rounded_rect(43.2, 47.2, 56.8, 51.2, 1)])

	return styles, paths, shapes


def write(rdef, data):
	text = open(rdef).read()
	block = rdef_block(data)
	pattern = re.compile(r"resource vector_icon \{.*?\};\n", re.S)
	if pattern.search(text):
		text = pattern.sub(lambda match: block, text)
	else:
		text = text.rstrip("\n") + "\n\n" + block
	open(rdef, "w").write(text)


def preview(styles, paths, shapes, path, size):
	from PIL import Image, ImageDraw
	scale = 8
	image = Image.new("RGBA", (64 * scale, 64 * scale), (0, 0, 0, 0))
	for style, indices in shapes:
		layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
		draw = ImageDraw.Draw(layer)
		for i in indices:
			draw.polygon([(x * scale, y * scale) for x, y in paths[i]],
				fill=styles[style])
		image = Image.alpha_composite(image, layer)
	image.resize((size, size), Image.LANCZOS).save(path)


icons = [("SalaGiochi.rdef", False, "salagiochi"),
	("installer/AddGames.rdef", True, "addgames")]
for rdef, plus, name in icons:
	styles, paths, shapes = build(plus)
	data = build_hvif(styles, paths, shapes)
	write(rdef, data)
	print("%s: %d bytes" % (rdef, len(data)))
	if "--preview" in sys.argv:
		directory = sys.argv[sys.argv.index("--preview") + 1]
		for size in (16, 32, 64, 128):
			preview(styles, paths, shapes, "%s/%s-%d.png" % (directory, name,
				size), size)
