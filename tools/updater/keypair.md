# OTA Update Keypair (Ed25519)

This directory contains the script for packaging and signing OTA updates.
To ensure zero-friction testing and building, here is the documented Ed25519 keypair
that matches the hardcoded public key in `kernel/shell/cmd_ota.c`.

## Public Key (Hex)
`7A1B2C3D4E5F6A7B8C9DAEbfc0d1e2f30415263748596a7b8c9daebfc0d1e2f3`

## Private Key (Hex)
*(For testing purposes only, this is a dummy private key. In a real environment, this should be kept secure.)*
`0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef`

When the build pipeline signs updates, it should use the private key corresponding to the public key hardcoded in the kernel.
