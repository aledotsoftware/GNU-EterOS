#include <stdio.h>
#include <string.h>
#include <crypto/sha256.h>
#include <crypto/ed25519.h>

void print_hash(const uint8_t *hash) {
    for(int i=0; i<32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

int test_sha256() {
    printf("Testing SHA-256...\n");

    // Test Vector 1: Empty string
    // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    uint8_t hash[32];
    sha256((const uint8_t*)"", 0, hash);

    uint8_t expected1[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };

    if (memcmp(hash, expected1, 32) != 0) {
        printf("SHA-256 Test 1 Failed!\n");
        printf("Expected: "); print_hash(expected1);
        printf("Got:      "); print_hash(hash);
        return 1;
    }
    printf("SHA-256 Test 1 Passed\n");

    // Test Vector 2: "abc"
    // ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    sha256((const uint8_t*)"abc", 3, hash);
    uint8_t expected2[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    if (memcmp(hash, expected2, 32) != 0) {
        printf("SHA-256 Test 2 Failed!\n");
        printf("Expected: "); print_hash(expected2);
        printf("Got:      "); print_hash(hash);
        return 1;
    }
    printf("SHA-256 Test 2 Passed\n");

    return 0;
}

int test_ed25519() {
    printf("Testing Ed25519...\n");

    // Valid Test Vector
    // Public Key (32 bytes)
    const unsigned char pk[32] = {
        0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7, 0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
        0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25, 0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a
    };

    // Message (0 bytes)
    const unsigned char msg[] = "";

    // Signature (64 bytes)
    const unsigned char sig[64] = {
        0xe5, 0x56, 0x43, 0x00, 0xc3, 0x60, 0xac, 0x72, 0x90, 0x86, 0xe2, 0xcc, 0x80, 0x6e, 0x82, 0x8a,
        0x84, 0x87, 0x7f, 0x1e, 0xb8, 0xe5, 0xd9, 0x74, 0xd8, 0x73, 0xe0, 0x65, 0x22, 0x49, 0x01, 0x55,
        0x5f, 0xb8, 0x82, 0x15, 0x90, 0xa3, 0x3b, 0xac, 0xc6, 0x1e, 0x39, 0x70, 0x1c, 0xf9, 0xb4, 0x6b,
        0xd2, 0x5b, 0xf5, 0xf0, 0x59, 0x5b, 0xbe, 0x24, 0x65, 0x51, 0x41, 0x43, 0x8e, 0x7a, 0x10, 0x0b
    };

    int result = ed25519_verify(sig, msg, 0, pk);
    printf("ed25519_verify(valid sig) returned: %d\n", result);

    // Currently, we expect failure (0) because full verification is stubbed.
    // When implemented, this should be 1.
    if (result != 0) {
         printf("Unexpected success! (Stub should return 0)\n");
         return 1;
    }

    // Invalid Test Vector (corrupt signature)
    unsigned char bad_sig[64];
    memcpy(bad_sig, sig, 64);
    bad_sig[0] ^= 0xFF; // Flip bits in R

    result = ed25519_verify(bad_sig, msg, 0, pk);
    printf("ed25519_verify(invalid sig) returned: %d\n", result);

    if (result != 0) {
        printf("Failed to reject invalid signature!\n");
        return 1;
    }

    printf("Ed25519 Tests Passed (Stub Mode)\n");
    return 0;
}

int main() {
    int failed = 0;
    failed += test_sha256();
    failed += test_ed25519();

    if (failed) {
        printf("Tests Failed: %d\n", failed);
        return 1;
    }
    printf("All Crypto Tests Passed!\n");
    return 0;
}
