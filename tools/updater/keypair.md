# OTA Update Keypair (Ed25519)

This directory contains the script for packaging and signing OTA updates.
To ensure zero-friction testing and building, here is the documented Ed25519 keypair
that matches the hardcoded public key in `kernel/shell/cmd_ota.c`.

## Public Key (Hex)
`207a067892821e25d770f1fba0c47c11ff4b813e54162ece9eb839e076231ab6`

## Private Key (Hex)
*(For testing purposes only, this is a dummy private key. In a real environment, this should be kept secure.)*
`0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef`

When the build pipeline signs updates, it should use the private key corresponding to the public key hardcoded in the kernel.
