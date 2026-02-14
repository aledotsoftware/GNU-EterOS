import struct
import math
import os

def draw_point(pixels, x, y, width, height, r, g, b, alpha):
    if 0 <= x < width and 0 <= y < height:
        idx = y * width + x
        pixels[idx] = struct.pack('<BBBB', b, g, r, alpha)

def generate_logo(output_path):
    width = 200
    height = 200
    # Initialize with transparent white
    pixels = [struct.pack('<BBBB', 255, 255, 255, 0)] * (width * height)

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
                
                # Outer Rim: Cyan (0x00B0B0 -> 0, 176, 176)
                # Outer Mid: Violet (0x9D00FF -> 157, 0, 255)
                # Inner: Deep Violet
                
                if ratio > 0.8:
                    r, g, b = 0, 176, 176 # Cyan Rim
                else:
                    r, g, b = 157, 0, 255 # Violet Body
                
                # Simple antialiasing
                alpha = 255
                if dist > radius - 2:
                    alpha = int(255 * (radius - dist) / 2)
                
                pixels[y * width + x] = struct.pack('<BBBB', b, g, r, alpha)

    # Draw stylized 'E' in the center
    # 'E' coordinates relative to 100,100
    # vertical bar: x: 80 to 90, y: 60 to 140
    # top bar: x: 90 to 130, y: 60 to 70
    # mid bar: x: 90 to 120, y: 95 to 105
    # bot bar: x: 90 to 130, y: 130 to 140
    
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
                # White 'E' with slight glow/shadow? No, keep it clean white.
                pixels[y * width + x] = struct.pack('<BBBB', 255, 255, 255, 255)

    with open(output_path, 'wb') as f:
        f.write(struct.pack('<II', width, height))
        for p in pixels:
            f.write(p)

if __name__ == "__main__":
    dest_dir = "initrd_root"
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    
    generate_logo(os.path.join(dest_dir, "logo.raw"))
    print("Logo generated with 'E' inside: initrd_root/logo.raw")
