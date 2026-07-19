#include <crypto/ed25519.h>
#include <crypto/tweetnacl.h>
#include <string.h>

#ifdef __ETEROS_HOST_TEST__
#include <stdlib.h>
#define kmalloc malloc
#define kfree free
#else
#include <mm.h>
#endif

/* ==========================================================================
 * Ed25519 Implementation
 * ========================================================================== */

int ed25519_verify(const unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key) {
    if (!signature || (!message && message_len > 0) || !public_key) {
        return 0; // Fail on invalid pointers
    }

    // Allocate buffer for signed message (signature + message)
    unsigned char *sm = kmalloc(64 + message_len);
    if (!sm) return 0;

    memcpy(sm, signature, 64);
    if (message_len > 0) {
        memcpy(sm + 64, message, message_len);
    }

    // Allocate buffer for output message
    unsigned char *mout = kmalloc(64 + message_len);
    if (!mout) {
        kfree(sm);
        return 0;
    }

    unsigned long long mlen_out = 0;

    // Call tweetnacl verify
    int res = crypto_sign_ed25519_tweet_open(mout, &mlen_out, sm, 64 + message_len, public_key);

    kfree(sm);
    kfree(mout);

    return (res == 0) ? 1 : 0;
}

void ed25519_publickey(const unsigned char *secret_key, unsigned char *public_key) {
    if (!secret_key || !public_key) return;
}

void ed25519_sign(unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key, const unsigned char *secret_key) {
    if (!signature || (!message && message_len > 0) || !public_key || !secret_key) return;
}
