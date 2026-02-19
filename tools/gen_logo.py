import struct
import math
import os
import zlib

def make_png(width, height, pixels):
    # pixels is a flat bytearray with RGBA data
    
    # Header
    png_sig = b'\x89PNG\r\n\x1a\n'
    
    # IHDR: width, height, bit_depth=8, color_type=6 (RGBA), compression=0, filter=0, interlace=0
    ihdr_data = struct.pack('!IIBBBBB', width, height, 8, 6, 0, 0, 0)
    ihdr_crc = zlib.crc32(ihdr_data, zlib.crc32(b'IHDR')) & 0xFFFFFFFF
    ihdr = struct.pack('!I', len(ihdr_data)) + b'IHDR' + ihdr_data + struct.pack('!I', ihdr_crc)
    
    # IDAT
    scanlines = []
    stride = width * 4
    for y in range(height):
        # Filter type 0 (None) at start of each scanline
        row_start = y * stride
        row_end = row_start + stride
        # Prepend filter byte to the row data
        line = b'\x00' + pixels[row_start:row_end]
        scanlines.append(line)
    
    raw_data = b''.join(scanlines)
    compressed = zlib.compress(raw_data)
    
    idat_crc = zlib.crc32(compressed, zlib.crc32(b'IDAT')) & 0xFFFFFFFF
    idat = struct.pack('!I', len(compressed)) + b'IDAT' + compressed + struct.pack('!I', idat_crc)
    
    # IEND
    iend_crc = zlib.crc32(b'', zlib.crc32(b'IEND')) & 0xFFFFFFFF
    iend = struct.pack('!I', 0) + b'IEND' + struct.pack('!I', iend_crc)
    
    return png_sig + ihdr + idat + iend

def generate_logo(output_path):
    width = 200
    height = 200
    # Initialize with transparent (all zeros)
    # Using bytearray is much faster and memory efficient than list of tuples
    pixels = bytearray(width * height * 4)

    cx, cy = 100, 100
    radius = 80
    radius_sq = radius * radius

    # Optimize: Iterate only within the bounding box of the circle
    # Original loop was 0..height, 0..width.
    # Bounding box is [cy-radius, cy+radius] and [cx-radius, cx+radius]
    min_y = max(0, cy - radius)
    max_y = min(height, cy + radius + 1)
    min_x = max(0, cx - radius)
    max_x = min(width, cx + radius + 1)

    for y in range(min_y, max_y):
        dy = y - cy
        dy_sq = dy * dy
        y_offset = y * width * 4

        for x in range(min_x, max_x):
            dx = x - cx
            dist_sq = dx*dx + dy_sq
            
            if dist_sq < radius_sq:
                dist = math.sqrt(dist_sq)
                # Flux Orb Gradient (Premium)
                ratio = dist / radius
                
                if ratio > 0.8:
                    r, g, b = 0, 176, 176 # Cyan Rim
                else:
                    r, g, b = 157, 0, 255 # Violet Body
                
                # Simple antialiasing
                alpha = 255
                if dist > radius - 2:
                    alpha = int(255 * (radius - dist) / 2)
                
                idx = y_offset + (x * 4)
                pixels[idx] = r
                pixels[idx+1] = g
                pixels[idx+2] = b
                pixels[idx+3] = alpha

    # Draw stylized 'E'
    for y in range(60, 141):
        y_offset = y * width * 4
        for x in range(80, 131):
            is_e = False
            # Vertical spine
            if 80 <= x <= 92: is_e = True
            # Horizontal bars
            if 60 <= y <= 72 and 80 <= x <= 130: is_e = True
            if 94 <= y <= 106 and 80 <= x <= 120: is_e = True
            if 128 <= y <= 140 and 80 <= x <= 130: is_e = True
            
            if is_e:
                idx = y_offset + (x * 4)
                pixels[idx] = 255
                pixels[idx+1] = 255
                pixels[idx+2] = 255
                pixels[idx+3] = 255

    png_data = make_png(width, height, pixels)
    
    with open(output_path, 'wb') as f:
        f.write(png_data)

if __name__ == "__main__":
    dest_dir = "initrd_root"
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    
    # Ensure raw file is cleaned up if it exists, to avoid confusion
    raw_path = os.path.join(dest_dir, "logo.raw")
    if os.path.exists(raw_path):
        os.remove(raw_path)
        
    output_path = os.path.join(dest_dir, "logo.png")
    generate_logo(output_path)
    print(f"Logo generated: {output_path}")
