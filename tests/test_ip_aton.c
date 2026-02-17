#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Vulnerable implementation copied from kernel/apps/wget.c */
uint32_t ip_aton_vulnerable(const char* cp) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t part = 0;
        while (*cp >= '0' && *cp <= '9') {
            part = part * 10 + (*cp - '0');
            cp++;
        }
        val |= (part << (i * 8));
        if (*cp == '.') cp++;
    }
    return val;
}

/* Proposed secure implementation */
uint32_t ip_aton_secure(const char* cp) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t part = 0;
        while (*cp >= '0' && *cp <= '9') {
            part = part * 10 + (*cp - '0');
            if (part > 255) return 0; /* Validation overflow */
            cp++;
        }
        val |= (part << (i * 8));
        if (i < 3) {
            if (*cp == '.') cp++;
            else return 0; /* Validation format: expected dot */
        }
    }
    return val;
}

int main() {
    printf("Testing ip_aton vulnerability...\n");

    /* Test Case 1: Normal IP */
    /* 1.2.3.4 -> 0x04030201 (Little Endian) */
    uint32_t res_vuln = ip_aton_vulnerable("1.2.3.4");
    uint32_t res_sec = ip_aton_secure("1.2.3.4");
    printf("1.2.3.4 -> Vuln: 0x%08X, Secure: 0x%08X\n", res_vuln, res_sec);
    assert(res_vuln == 0x04030201);
    assert(res_sec == 0x04030201);

    /* Test Case 2: Overflow */
    /* 300.2.3.4 */
    /* Vuln: 300 = 0x12C. 0x12C & 0xFF = 0x2C. Or full 0x12C if masked? */
    /* The vulnerable code does: val |= (part << (i*8)).
       part = 300 (0x12C). i=0. val |= 0x12C.
       So byte 0 becomes 0x2C, but byte 1 gets 0x01 OR'd into it?
       If next part is 2. val |= 2 << 8 (0x200).
       val was 0x12C. 0x12C | 0x200 = 0x32C.
       So byte 1 is 0x01 | 0x02 = 0x03.
       So 300.2... becomes 44.3... effectively? (0x2C, 0x03)
       Let's see.
    */
    res_vuln = ip_aton_vulnerable("300.2.3.4");
    res_sec = ip_aton_secure("300.2.3.4");
    printf("300.2.3.4 -> Vuln: 0x%08X, Secure: 0x%08X\n", res_vuln, res_sec);

    /* We expect secure version to return 0 (error) */
    assert(res_sec == 0);
    /* Vuln version returns garbage */

    /* Test Case 3: Overflow Logic Check */
    /* 256.0.0.0 -> 256 is 0x100. */
    /* i=0: val |= 0x100. val = 0x100. */
    /* Result: 0.1.0.0 (in IP terms) */
    res_vuln = ip_aton_vulnerable("256.0.0.0");
    printf("256.0.0.0 -> Vuln: 0x%08X (IP: %d.%d.%d.%d)\n",
           res_vuln, res_vuln&0xFF, (res_vuln>>8)&0xFF, (res_vuln>>16)&0xFF, (res_vuln>>24)&0xFF);

    /* This confirms 256 is interpreted as 0.1.0.0 in Little Endian view or whatever.
       Actually 0x100 is: byte 0 = 0x00, byte 1 = 0x01.
       So yes, 256.0.0.0 becomes 0.1.0.0. This is a bypass.
    */

    /* Test Case 4: Missing dots */
    /* "1.2.3" (incomplete) */
    /* Vuln loop runs 4 times. */
    /* i=0: 1. cp skips dot. */
    /* i=1: 2. cp skips dot. */
    /* i=2: 3. cp is at '\0'. Loop continues? */
    /* while(*cp >= '0' && *cp <= '9') fails immediately. part=0. */
    /* val |= 0 << 16. */
    /* if (*cp == '.') fails. */
    /* i=3: part=0. */
    /* Result: 1.2.3.0 */
    res_vuln = ip_aton_vulnerable("1.2.3");
    res_sec = ip_aton_secure("1.2.3"); /* Should be valid? No, loop expects 4 parts */
    /* Actually secure version loop 4 times.
       i=0: 1. ok.
       i=1: 2. ok.
       i=2: 3. ok. cp points to null.
       if (*cp == '.') fails! returns 0. CORRECT.
    */
    printf("1.2.3 -> Vuln: 0x%08X, Secure: 0x%08X\n", res_vuln, res_sec);
    assert(res_sec == 0);


    /* Test Case 5: Garbage at end */
    /* "1.2.3.4.5" */
    /* Vuln:
       i=3: 4. val |= ... cp points to .5
       if (*cp == '.') cp++.
       Loop finishes.
       Returns 1.2.3.4.
       Ideally we should check if string ends? But standard inet_aton ignores trailing garbage often?
       Strict validation would check for null terminator.
       My secure implementation doesn't check for null terminator after loop, but checks structure inside.
    */

    printf("Tests finished.\n");
    return 0;
}
