#!/usr/bin/env python3
import struct
import sys
import os
import math

# Constants
SECTOR_SIZE = 512
CLUSTER_SIZE = 512 # 1 sector per cluster
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 32
FAT_COUNT = 2
ROOT_CLUSTER = 2
MEDIA_TYPE = 0xF8

# FAT32 Entry Types
ATTR_READ_ONLY = 0x01
ATTR_HIDDEN    = 0x02
ATTR_SYSTEM    = 0x04
ATTR_VOLUME_ID = 0x08
ATTR_DIRECTORY = 0x10
ATTR_ARCHIVE   = 0x20
ATTR_LONG_NAME = 0x0F

def create_fat32_image(output_file, files):
    # 1. Calculate Sizes
    total_data_size = 0
    parsed_files = []

    for arg in files:
        if ':' in arg:
            src, dst = arg.split(':')
        else:
            src = arg
            dst = os.path.basename(arg)

        if os.path.exists(src):
            total_data_size += os.path.getsize(src)
            parsed_files.append((src, dst))
        else:
            print(f"Warning: File {src} not found, skipping.")

    # Add overhead for directories
    total_data_size += 16 * CLUSTER_SIZE

    # Minimum image size (64MB)
    image_size = max(64 * 1024 * 1024, total_data_size + 4 * 1024 * 1024)
    total_sectors = image_size // SECTOR_SIZE

    # Calculate FAT Size
    num_clusters = (total_sectors - RESERVED_SECTORS) // SECTORS_PER_CLUSTER
    fat_size_bytes = num_clusters * 4
    fat_sectors = (fat_size_bytes + SECTOR_SIZE - 1) // SECTOR_SIZE

    # Data Start Sector
    data_start_sector = RESERVED_SECTORS + (FAT_COUNT * fat_sectors)

    print(f"Creating FAT32 Image: {output_file}")
    print(f"  Size: {image_size} bytes")
    print(f"  Sectors: {total_sectors}")
    print(f"  FAT Size: {fat_sectors} sectors")
    print(f"  Data Start: {data_start_sector}")

    with open(output_file, 'wb') as f:
        # Fill with zeros
        f.seek(image_size - 1)
        f.write(b'\0')
        f.seek(0)

        # ---------------------------------------------------------------------
        # 1. Boot Sector (Sector 0)
        # ---------------------------------------------------------------------
        bpb = bytearray(512)
        bpb[0:3] = b'\xEB\x58\x90'
        bpb[3:11] = b'MSDOS5.0'
        struct.pack_into('<H', bpb, 11, SECTOR_SIZE)
        bpb[13] = SECTORS_PER_CLUSTER
        struct.pack_into('<H', bpb, 14, RESERVED_SECTORS)
        bpb[16] = FAT_COUNT
        struct.pack_into('<H', bpb, 17, 0)
        struct.pack_into('<H', bpb, 19, 0)
        bpb[21] = MEDIA_TYPE
        struct.pack_into('<H', bpb, 22, 0)
        struct.pack_into('<H', bpb, 24, 63)
        struct.pack_into('<H', bpb, 26, 255)
        struct.pack_into('<I', bpb, 28, 0)
        struct.pack_into('<I', bpb, 32, total_sectors)

        # FAT32 Extended BPB
        struct.pack_into('<I', bpb, 36, fat_sectors)
        struct.pack_into('<H', bpb, 40, 0)
        struct.pack_into('<H', bpb, 42, 0)
        struct.pack_into('<I', bpb, 44, ROOT_CLUSTER)
        struct.pack_into('<H', bpb, 48, 1)
        struct.pack_into('<H', bpb, 50, 6)

        bpb[510:512] = b'\x55\xAA'
        f.write(bpb)

        # ---------------------------------------------------------------------
        # 2. FSInfo (Sector 1)
        # ---------------------------------------------------------------------
        fsinfo = bytearray(512)
        struct.pack_into('<I', fsinfo, 0, 0x41615252)
        struct.pack_into('<I', fsinfo, 484, 0x61417272)
        struct.pack_into('<I', fsinfo, 488, num_clusters - 1)
        struct.pack_into('<I', fsinfo, 492, 3) # Next free is 3
        struct.pack_into('<I', fsinfo, 508, 0xAA550000)
        f.write(fsinfo)

        f.seek(RESERVED_SECTORS * SECTOR_SIZE)

        # ---------------------------------------------------------------------
        # 3. FAT Tables Setup
        # ---------------------------------------------------------------------
        fat_table = [0] * num_clusters
        fat_table[0] = 0x0FFFFFF8
        fat_table[1] = 0x0FFFFFFF
        fat_table[2] = 0x0FFFFFFF # Root Cluster EOC

        # ---------------------------------------------------------------------
        # 4. Data Area
        # ---------------------------------------------------------------------
        next_free_cluster = 3 # Start after Root

        def alloc_cluster():
            nonlocal next_free_cluster
            c = next_free_cluster
            next_free_cluster += 1
            if c < len(fat_table):
                fat_table[c] = 0x0FFFFFFF
            return c

        def chain_cluster(prev, current):
            if prev < len(fat_table):
                fat_table[prev] = current

        def write_cluster(cluster_idx, data):
            offset = (data_start_sector * SECTOR_SIZE) + ((cluster_idx - 2) * CLUSTER_SIZE)
            f.seek(offset)
            f.write(data)

        # Structure to track directories
        dirs = {
            '': [],
            'EFI': [],
            'EFI/BOOT': []
        }

        # Process Files
        for src_path, dst_path in parsed_files:
            file_size = os.path.getsize(src_path)

            with open(src_path, 'rb') as src_f:
                file_data = src_f.read()

            chunks = [file_data[i:i+CLUSTER_SIZE] for i in range(0, len(file_data), CLUSTER_SIZE)]
            if not chunks: chunks = [b'']

            first_cluster = 0
            prev_cluster = 0

            for i, chunk in enumerate(chunks):
                cluster = alloc_cluster()
                if i == 0: first_cluster = cluster
                else: chain_cluster(prev_cluster, cluster)

                write_cluster(cluster, chunk.ljust(CLUSTER_SIZE, b'\0'))
                prev_cluster = cluster

            parts = dst_path.replace('\\', '/').split('/')
            filename = parts[-1]
            parent_dir = '/'.join(parts[:-1])

            name_part = filename.split('.')
            name = name_part[0][:8].upper().ljust(8)
            ext = (name_part[1][:3].upper() if len(name_part) > 1 else "").ljust(3)

            if parent_dir in dirs:
                dirs[parent_dir].append({
                    'name': name.encode('ascii'),
                    'ext': ext.encode('ascii'),
                    'attr': ATTR_ARCHIVE,
                    'cluster': first_cluster,
                    'size': file_size
                })
            else:
                print(f"Warning: Directory {parent_dir} not supported in simple structure. Placing in Root.")
                dirs[''].append({
                    'name': name.encode('ascii'),
                    'ext': ext.encode('ascii'),
                    'attr': ATTR_ARCHIVE,
                    'cluster': first_cluster,
                    'size': file_size
                })

        # Create Directories (Bottom-Up)

        # 1. EFI/BOOT
        efi_boot_cluster = alloc_cluster()
        # . point to self, .. point to parent (0 for now, will fix)

        def create_dir_entry(name, ext, attr, cluster, size):
            # 8s(Name) 3s(Ext) B(Attr) B(NT) B(CrtTen) H(CrtTime) H(CrtDate) H(AccDate) H(High) H(WrtTime) H(WrtDate) H(Low) I(Size)
            return struct.pack('<8s3sBBBHHHHHHHI', name, ext, attr, 0, 0, 0, 0, 0, cluster >> 16, 0, 0, cluster & 0xFFFF, size)

        dir_data = bytearray(CLUSTER_SIZE)
        dir_data[0:32]   = create_dir_entry(b'.       ', b'   ', ATTR_DIRECTORY, efi_boot_cluster, 0)
        dir_data[32:64]  = create_dir_entry(b'..      ', b'   ', ATTR_DIRECTORY, 0, 0) # Placeholder

        offset = 64
        for entry in dirs['EFI/BOOT']:
            dir_data[offset:offset+32] = create_dir_entry(entry['name'], entry['ext'], entry['attr'], entry['cluster'], entry['size'])
            offset += 32
        write_cluster(efi_boot_cluster, dir_data)

        # 2. EFI
        efi_cluster = alloc_cluster()

        # Fix .. in EFI/BOOT to point to EFI
        dir_data[32:64] = create_dir_entry(b'..      ', b'   ', ATTR_DIRECTORY, efi_cluster, 0)
        write_cluster(efi_boot_cluster, dir_data) # Update EFI/BOOT

        dir_data = bytearray(CLUSTER_SIZE)
        dir_data[0:32]   = create_dir_entry(b'.       ', b'   ', ATTR_DIRECTORY, efi_cluster, 0)
        dir_data[32:64]  = create_dir_entry(b'..      ', b'   ', ATTR_DIRECTORY, 0, 0) # Placeholder (Root)

        # Add BOOT entry
        dir_data[64:96]  = create_dir_entry(b'BOOT    ', b'   ', ATTR_DIRECTORY, efi_boot_cluster, 0)

        offset = 96
        for entry in dirs['EFI']:
             dir_data[offset:offset+32] = create_dir_entry(entry['name'], entry['ext'], entry['attr'], entry['cluster'], entry['size'])
             offset += 32
        write_cluster(efi_cluster, dir_data)

        # 3. Root (Cluster 2)
        # Fix .. in EFI to point to Root (Cluster 0 usually for Root in FAT32 .. entries)
        # Actually in FAT32, .. pointing to Root is cluster 0.

        dir_data = bytearray(CLUSTER_SIZE)
        offset = 0

        # Add EFI entry
        dir_data[offset:offset+32] = create_dir_entry(b'EFI     ', b'   ', ATTR_DIRECTORY, efi_cluster, 0)
        offset += 32

        for entry in dirs['']:
             dir_data[offset:offset+32] = create_dir_entry(entry['name'], entry['ext'], entry['attr'], entry['cluster'], entry['size'])
             offset += 32

        write_cluster(ROOT_CLUSTER, dir_data)

        # ---------------------------------------------------------------------
        # 5. Write FAT Tables
        # ---------------------------------------------------------------------
        fat_bytes = bytearray(fat_sectors * SECTOR_SIZE)
        for i, val in enumerate(fat_table):
            if i < len(fat_table):
                struct.pack_into('<I', fat_bytes, i * 4, val)

        f.seek(RESERVED_SECTORS * SECTOR_SIZE)
        f.write(fat_bytes) # FAT1
        f.write(fat_bytes) # FAT2

    print("Done.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: mkuefi.py <output_img> <files...>")
        sys.exit(1)

    output = sys.argv[1]
    files = sys.argv[2:]
    create_fat32_image(output, files)
