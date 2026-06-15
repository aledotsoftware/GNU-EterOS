#!/usr/bin/env python3
import sys
import hashlib
import os

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_file> <output_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    with open(input_file, "rb") as f:
        data = f.read()

    # Create the 64-byte mock Ed25519 signature
    # In EterOS, the mock ed25519_verify checks if the 64-byte signature equals the SHA-512 hash
    # of the message, provided the OTA public key is used.
    # Therefore, we just hash the payload and pad it or truncate it to 64 bytes. (SHA-512 is 64 bytes natively).
    hasher = hashlib.sha512()
    hasher.update(data)
    sig = hasher.digest()

    assert len(sig) == 64

    with open(output_file, "wb") as f:
        f.write(sig)
        f.write(data)

    print(f"Successfully packed {input_file} into {output_file} ({len(data)} bytes data, 64 bytes signature).")

if __name__ == "__main__":
    main()
