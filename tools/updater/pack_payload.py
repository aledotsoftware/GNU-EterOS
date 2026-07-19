#!/usr/bin/env python3
import sys
import os
import binascii
import nacl.signing

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_file> <output_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    with open(input_file, "rb") as f:
        data = f.read()

    # Seed for Ed25519 Keypair (must match the documented seed and public key in kernel)
    seed_hex = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    seed = binascii.unhexlify(seed_hex)
    signing_key = nacl.signing.SigningKey(seed)

    # Sign the payload (returns SignedMessage object, where signature is the first 64 bytes)
    signed = signing_key.sign(data)
    sig = signed.signature

    assert len(sig) == 64

    with open(output_file, "wb") as f:
        f.write(sig)
        f.write(data)

if __name__ == "__main__":
    main()
