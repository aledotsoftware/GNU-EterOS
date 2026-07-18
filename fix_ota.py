with open('kernel/shell/cmd_ota.c', 'r') as f:
    content = f.read()

# We need to remove the duplicate `if (payload_size < 64)` check and also fix the `payload_size < data_offset` check. Wait, `data_offset = 64`.
# `if (payload_size < data_offset)` is equivalent to `if (payload_size < 64)`.
