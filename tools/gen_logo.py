import struct
import math
import os
import zlib

def make_png(width, height, pixels):
    # pixels is a list of (r,g,b,a) tuples
    
    # Header
    png_sig = b'\x89PNG\r\n\x1a\n'
    
    # IHDR: width, height, bit_depth=8, color_type=6 (RGBA), compression=0, filter=0, interlace=0
    ihdr_data = struct.pack('!IIBBBBB', width, height, 8, 6, 0, 0, 0)
    ihdr_crc = zlib.crc32(ihdr_data, zlib.crc32(b'IHDR')) & 0xFFFFFFFF
    ihdr = struct.pack('!I', len(ihdr_data)) + b'IHDR' + ihdr_data + struct.pack('!I', ihdr_crc)
    
    # IDAT
    scanlines = []
    for y in range(height):
        # Filter type 0 (None) at start of each scanline
        line = bytearray([0])
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            line.append(r)
            line.append(g)
            line.append(b)
            line.append(a)
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
    # Initialize with transparent
    pixels = [(0, 0, 0, 0)] * (width * height)

    cx, cy = 100, 100
    radius = 80

    for y in range(height):
        for x in range(width):
            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx*dx + dy*dy)
            
            if dist < radius:
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
                
                pixels[y * width + x] = (r, g, b, alpha)

    # Draw stylized 'E'
    for y in range(60, 141):
        for x in range(80, 131):
            is_e = False
            # Vertical spine
            if 80 <= x <= 92: is_e = True
            # Horizontal bars
            if 60 <= y <= 72 and 80 <= x <= 130: is_e = True
            if 94 <= y <= 106 and 80 <= x <= 120: is_e = True
            if 128 <= y <= 140 and 80 <= x <= 130: is_e = True
            
            if is_e:
                pixels[y * width + x] = (255, 255, 255, 255)

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
