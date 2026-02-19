import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Error: Pillow (PIL) is required to generate the logo.")
    print("Please install it using: pip install Pillow")
    sys.exit(1)

def generate_logo(output_path):
    # Dimensions
    final_width, final_height = 200, 200
    scale = 4  # Supersampling for antialiasing
    width = final_width * scale
    height = final_height * scale
    
    # Create transparent image
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Helper to scale coordinates
    def s(val):
        return val * scale
    
    cx, cy = s(100), s(100)
    radius = s(80)
    
    # Flux Orb Gradient (Premium) -> Approximated by two circles
    # Outer Cyan Circle (Rim)
    # Original logic: ratio > 0.8 -> Cyan. Ratio = dist/radius.
    # This means the outer ring (from 0.8*R to R) is Cyan.
    
    # Draw Cyan Circle (Outer)
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=(0, 176, 176))
    
    # Inner Violet Circle (Body)
    # Radius = 0.8 * 80 = 64
    inner_radius = s(64)
    draw.ellipse((cx - inner_radius, cy - inner_radius, cx + inner_radius, cy + inner_radius), fill=(157, 0, 255))
    
    # Draw stylized 'E'
    white = (255, 255, 255, 255)

    # Vertical spine: 80..92, 60..140
    # Original used inclusive ranges: range(80, 131) -> 80..130 inclusive
    # So we add 1 to the end coordinate to cover the full pixel
    draw.rectangle((s(80), s(60), s(93), s(141)), fill=white)

    # Top bar: 80..130, 60..72
    draw.rectangle((s(80), s(60), s(131), s(73)), fill=white)

    # Middle bar: 80..120, 94..106
    draw.rectangle((s(80), s(94), s(121), s(107)), fill=white)

    # Bottom bar: 80..130, 128..140
    draw.rectangle((s(80), s(128), s(131), s(141)), fill=white)

    # Resize with high quality downsampling
    img = img.resize((final_width, final_height), resample=Image.Resampling.LANCZOS)
    
    img.save(output_path, "PNG")

if __name__ == "__main__":
    dest_dir = "initrd_root"
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    
    # Cleanup raw file if it exists (legacy)
    raw_path = os.path.join(dest_dir, "logo.raw")
    if os.path.exists(raw_path):
        os.remove(raw_path)
        
    output_path = os.path.join(dest_dir, "logo.png")
    generate_logo(output_path)
    print(f"Logo generated: {output_path}")
