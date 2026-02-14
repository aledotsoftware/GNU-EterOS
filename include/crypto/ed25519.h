#ifndef CRYPTO_ED25519_H
#define CRYPTO_ED25519_H

#include <types.h>

/*
 * Ed25519 signature verification.
 *
 * signature: 64 bytes
 * message: message to verify
 * message_len: length of message
 * public_key: 32 bytes
 *
 * Returns 1 if signature is valid, 0 otherwise.
 */
int ed25519_verify(const unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key);

/*
 * Generates a public key from a secret key.
 * secret_key: 32 bytes
 * public_key: 32 bytes (output)
 */
void ed25519_publickey(const unsigned char *secret_key, unsigned char *public_key);

/*
 * Signs a message.
 * message: message to sign
 * message_len: length of message
 * public_key: 32 bytes
 * secret_key: 32 bytes
 * signature: 64 bytes (output)
 */
void ed25519_sign(unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key, const unsigned char *secret_key);

#endif
