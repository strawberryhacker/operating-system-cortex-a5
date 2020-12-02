# Copyright (C) strawberryhacker

import sys
import argparse
import glyphtools
import math
from PIL import Image, ImageFont, ImageDraw
from fontTools.ttLib import TTFont

# Parse arguments
parser = argparse.ArgumentParser(description='TinyFont generator')
parser.add_argument('-pt', nargs='+', type=int, required=True,
                    help='Point size for TinyFont - between 8 and 30')
parser.add_argument('-ttf', required = True, help='File path of the TTF file')
parser.add_argument('-tf', required = True, help='File name of TinyFont')
parser.add_argument('-prev', action="store_true", help="Generates preview")
args = parser.parse_args()

# Sizes of different fields
header_size       = 8
size_info_size    = 12
glyph_header_size = 4
glyph_info_size   = 12

# Do NOT touch these
start_char = 32
num_chars = 95

# Dict for the sizes
size_dict = []

# Aligned by design. Curr offset will point to the start of the first font
curr_offset = header_size + len(args.pt) * size_info_size

# This will be filled with the font data
font_data = bytearray()

# Iterate though all sizes
for pt in range(len(args.pt)):

    # Get the font size
    font_size = args.pt[pt]

    # Load the font
    font = ImageFont.truetype(args.ttf, font_size)
    font_info = TTFont(args.ttf)
    start_id = font_info.getGlyphID('space')
    scale = font_info['head'].unitsPerEm

    if args.prev:
        preview = ""
        for i in range(num_chars):
            preview += str(chr(i + start_char))
        image = Image.new("RGB", (1000, 1000))
        draw = ImageDraw.Draw(image)
        draw.text((0, 0), preview, font=font)
        image=image.crop(image.getbbox())
        image.save(args.tf + "_prev" + str(args.pt[pt]) + ".png")
        pixels = image.load()

    # Find the max and min values
    ymax = 0
    ymin = 0
    for z in range(num_chars):
        m = glyphtools.get_glyph_metrics(font_info, font_info.getGlyphName(start_id + z))
        new_ymax  = (font_size * m['yMax'] / scale)
        new_ymin  = (font_size * m['yMin'] / scale)
        ymin = round(min(ymin, new_ymin))
        ymax = round(max(ymax, new_ymax))
    maxmax = round(abs(ymin) + abs(ymax))

    # We are adding a new font size. Allign the start address
    if curr_offset & 3:
        curr_offset = (curr_offset + 3) & ~3
    
    # Add the glyph header indicating the global size of the font
    font_data += abs(ymin).to_bytes(1, byteorder="little", signed=False)
    font_data += abs(ymax).to_bytes(1, byteorder="little", signed=False)
    font_data.append(0)
    font_data.append(0)

    size_dict.append({"pt":font_size, "offset":curr_offset, "size":0})

    # Jump past the font header
    curr_offset += 4

    #---------------------------------------------------------------------------
    local_info = bytearray()
    local_data = bytearray()
    local_data_offset = curr_offset + num_chars * glyph_info_size
    local_data_size = 0
    #---------------------------------------------------------------------------

    for i in range(num_chars):
        char = str(chr(start_char + i))
        image = Image.new("RGB", (1000, 1000))
        draw = ImageDraw.Draw(image)
        draw.text((0, 0), char, font=font)
        image=image.crop(image.getbbox())
        pixels = image.load()

        # Get size of the image. If not cropped the bitmap is ignored
        x = image.size[0]
        y = image.size[1]
        if x > font_size or y > font_size:
            x = 0
            y = 0
        
        # Temporarily offset to the start of the data section
        offset = local_data_offset

        # Read the data
        local_data_size += x * y
        local_data_offset += x * y
        for a in range(0, y):
            for b in range(0, x):
                local_data.append(pixels[b, a][0])            

        m = glyphtools.get_glyph_metrics(font_info, font_info.getGlyphName(start_id + i))
        lsb   = (font_size * m['lsb'] / scale)
        rsb   = (font_size * m['rsb'] / scale)
        xMin  = (font_size * m['xMin'] / scale)
        xMax  = (font_size * m['xMax'] / scale)
        yMin  = (font_size * m['yMin'] / scale)
        yMax  = (font_size * m['yMax'] / scale)
        width = (font_size * m['width'] / scale)
        if round(yMin) > 0:
            y_off = maxmax - y - (abs(ymin) + abs(round(yMin)))
        else:
            y_off = maxmax - y - (abs(ymin) - abs(round(yMin)))
        x_off = round(lsb)
        x_adv = x + x_off

        # Add the information about a glyph
        local_info += offset.to_bytes(4, byteorder="little", signed=False)
        local_info += x.to_bytes(1, byteorder="little", signed=False)
        local_info += y.to_bytes(1, byteorder="little", signed=False)
        local_info += x_off.to_bytes(1, byteorder="little", signed=False)
        local_info += y_off.to_bytes(1, byteorder="little", signed=False)
        local_info += x_adv.to_bytes(1, byteorder="little", signed=False)
        local_info.append(0)
        local_info.append(0)
        local_info.append(0)

    
    font_data += local_info + local_data

    # Fill in the acctual data size. This is used if the OS only want to load 
    # a part of the size collection. Then the OS can allocate this size and 
    # copy the entire font
    size_dict[-1]["size"] = num_chars * glyph_info_size + local_data_size

    # Move the pointer to the next font
    curr_offset += local_data_size + num_chars * glyph_info_size

# Create a new file
font_file = open(args.tf + ".tf", "wb")
font_file.write(bytearray([0xCA, 0xCA, 0xCA, 0xCA, start_char, num_chars, len(args.pt), 0x00]))

# Add the size information
size_info = bytearray()
for i in range(len(args.pt)):
    size_info += size_dict[i]["pt"].to_bytes(4, byteorder="little", signed=False)
    size_info += size_dict[i]["offset"].to_bytes(4, byteorder="little", signed=False)
    size_info += size_dict[i]["size"].to_bytes(4, byteorder="little", signed=False)

font_file.write(size_info)
font_file.write(font_data)

font_file.close()
