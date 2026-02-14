#!/usr/bin/env python3
import struct
import os
import sys

def create_initrd(input_dir, output_file):
    files = []
    for root, _, filenames in os.walk(input_dir):
        for filename in filenames:
            files.append(os.path.join(root, filename))

    # Header: Magic (4) + Count (4)
    magic = b'INIT'
    count = len(files)
    header = struct.pack('<4sI', magic, count)

    entries = b''
    data = b''

    # Calculate offset of data start: Header + (EntrySize * Count)
    # EntrySize = 64 (name) + 4 (size) + 4 (offset) = 72 bytes
    data_start_offset = 8 + (72 * count)

    current_offset = data_start_offset

    for filepath in files:
        rel_path = os.path.relpath(filepath, input_dir)
        # Ensure path uses forward slashes
        rel_path = rel_path.replace(os.sep, '/')

        # Truncate filename if longer than 63 chars (leaving space for null terminator)
        if len(rel_path) > 63:
            print(f"Warning: Filename '{rel_path}' truncated to 63 chars.")
            rel_path = rel_path[:63]

        with open(filepath, 'rb') as f:
            content = f.read()
            size = len(content)

            # Entry: Name (64s), Size (I), Offset (I)
            # Pad name with null bytes
            entry = struct.pack('<64sII', rel_path.encode('utf-8'), size, current_offset)
            entries += entry

            data += content
            current_offset += size

    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(entries)
        f.write(data)

    print(f"Created initrd '{output_file}' with {count} files. Total size: {current_offset} bytes.")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: mkinitrd.py <input_dir> <output_file>")
        sys.exit(1)

    create_initrd(sys.argv[1], sys.argv[2])
