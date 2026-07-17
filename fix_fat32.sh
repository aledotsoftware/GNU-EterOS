sed -i 's/return -4; \/\/ No free space found/return -ENOSPC; \/\/ No free space found/g' kernel/fs/fat32.c
sed -i 's/if (fat32_update_dirent(vol, sector, offset, &entry) != 0) return -4;/if (fat32_update_dirent(vol, sector, offset, \&entry) != 0) return -EIO;/g' kernel/fs/fat32.c
