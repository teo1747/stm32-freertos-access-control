#include "test.h"
#include "crc16.h"
#include "sha256.h"

static void hex(const uint8_t *d, char *out)
{
    for (int i = 0; i < SHA256_SIZE; i++)
        snprintf(out + 2 * i, 3, "%02x", d[i]);
}

static void check(const char *msg, size_t n, const char *expect)
{
    uint8_t d[SHA256_SIZE];
    char s[2 * SHA256_SIZE + 1];
    sha256(msg, n, d);
    hex(d, s);
    CHECK_STR(s, expect);
}

int main(void)
{
    /* FIPS 180-4 examples and the classic vectors */
    check("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    /* One million 'a', fed in uneven pieces across block boundaries */
    struct sha256 s;
    uint8_t d[SHA256_SIZE];
    char text[2 * SHA256_SIZE + 1];
    char chunk[997];
    memset(chunk, 'a', sizeof chunk);
    sha256_init(&s);
    size_t left = 1000000;
    while (left) {
        size_t n = left < sizeof chunk ? left : sizeof chunk;
        sha256_update(&s, chunk, n);
        left -= n;
    }
    sha256_final(&s, d);
    hex(d, text);
    CHECK_STR(text, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    /* Every length around the padding boundary agrees with one-shot hashing */
    uint8_t a[SHA256_SIZE], b[SHA256_SIZE];
    for (size_t n = 50; n < 70; n++) {
        sha256(chunk, n, a);
        sha256_init(&s);
        for (size_t i = 0; i < n; i++)
            sha256_update(&s, chunk + i, 1);
        sha256_final(&s, b);
        CHECK(memcmp(a, b, sizeof a) == 0);
    }

    /* CRC-16/CCITT-FALSE's check value */
    CHECK_INT(crc16("123456789", 9), 0x29B1);
    CHECK_INT(crc16("", 0), 0xFFFF);

    return test_report("crypto");
}
