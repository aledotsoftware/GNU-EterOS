import math
import os
from PIL import Image

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

    # Use Pillow to create and save the image from raw bytes
    img = Image.frombuffer("RGBA", (width, height), pixels, "raw", "RGBA", 0, 1)
    img.save(output_path)

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
