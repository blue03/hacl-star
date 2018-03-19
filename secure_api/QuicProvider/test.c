#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "quic_provider.h"

void dump(char buffer[], size_t len)
{
  size_t i;
  for(i=0; i<len; i++) {
    printf("%02x",buffer[i] & 0xFF);
    if (i % 32 == 31 || i == len-1) printf("\n");
  }
}

// Older coverage tests
void coverage(void)
{
  printf("==== coverage ====\n");

  char hash[64] = {0};
  char input[1] = {0};
  printf("SHA256('') =\n");
  quic_crypto_hash(TLS_hash_SHA256, hash, input, 0);
  dump(hash, 32);
  printf("\nSHA384('') = \n");
  quic_crypto_hash(TLS_hash_SHA384, hash, input, 0);
  dump(hash, 48);
  printf("\nSHA512('') = \n");
  quic_crypto_hash(TLS_hash_SHA512, hash, input, 0);
  dump(hash, 64);

  char *key = "Jefe";
  char *data = "what do ya want for nothing?";

  printf("\nHMAC-SHA256('Jefe', 'what do ya want for nothing?') = \n");
  quic_crypto_hmac(TLS_hash_SHA256, hash, key, 4, data, 28);
  dump(hash, 32);
  assert(memcmp(hash, "\x5b\xdc\xc1\x46\xbf\x60\x75\x4e\x6a\x04\x24\x26\x08\x95\x75\xc7\x5a\x00\x3f\x08\x9d\x27\x39\x83\x9d\xec\x58\xb9\x64\xec\x38\x43", 32) == 0);

  printf("\nHMAC-SHA384('Jefe', 'what do ya want for nothing?') = \n");
  quic_crypto_hmac(TLS_hash_SHA384, hash, key, 4, data, 28);
  dump(hash, 48);
  assert(memcmp(hash, "\xaf\x45\xd2\xe3\x76\x48\x40\x31\x61\x7f\x78\xd2\xb5\x8a\x6b\x1b\x9c\x7e\xf4\x64\xf5\xa0\x1b\x47\xe4\x2e\xc3\x73\x63\x22\x44\x5e\x8e\x22\x40\xca\x5e\x69\xe2\xc7\x8b\x32\x39\xec\xfa\xb2\x16\x49", 48) == 0);

  printf("\nHMAC-SHA512('Jefe', 'what do ya want for nothing?') = \n");
  quic_crypto_hmac(TLS_hash_SHA512, hash, key, 4, data, 28);
  dump(hash, 64);
  assert(memcmp(hash, "\x16\x4b\x7a\x7b\xfc\xf8\x19\xe2\xe3\x95\xfb\xe7\x3b\x56\xe0\xa3\x87\xbd\x64\x22\x2e\x83\x1f\xd6\x10\x27\x0c\xd7\xea\x25\x05\x54\x97\x58\xbf\x75\xc0\x5a\x99\x4a\x6d\x03\x4f\x65\xf8\xf0\xe6\xfd\xca\xea\xb1\xa3\x4d\x4a\x6b\x4b\x63\x6e\x07\x0a\x38\xbc\xe7\x37", 64) == 0);

  char *salt = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c";
  char *ikm = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
  char *info = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9";

  printf("\nprk = HKDF-EXTRACT-SHA256('0x000102030405060708090a0b0c', '0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b')\n");
  quic_crypto_hkdf_extract(TLS_hash_SHA256, hash, salt, 13, ikm, 22);
  dump(hash, 32);

  char prk[32] = {0};
  memcpy(prk, hash, 32);
  dump(prk, 32);

  char okm[42] = {0};
  printf("\nokm = HKDF-EXPAND-SHA256(prk, '0xf0f1f2f3f4f5f6f7f8f9', 42)\n");
  if(!quic_crypto_hkdf_expand(TLS_hash_SHA256, okm, 42, prk, 32, info, 10))
  {
    printf("Failed to call HKDF-expand\n");
    exit(1);
  }
  dump(okm, 42);

  quic_secret s = { .hash = TLS_hash_SHA256, .ae = TLS_aead_AES_128_GCM };
  memcpy(s.secret, hash, 32);
  quic_crypto_tls_derive_secret(&s, &s, "EXPORTER-QUIC server 1-RTT Secret");

  quic_key* k;
  if(!quic_crypto_derive_key(&k, &s))
  {
    printf("Failed to derive key\n");
    exit(1);
  }

  char cipher[128];
  printf("\nAES-128-GCM encrypt test:\n");
  quic_crypto_encrypt(k, cipher, 0, salt, 13, data, 28);
  dump(cipher, 28+16);

  if(quic_crypto_decrypt(k, hash, 0, salt, 13, cipher, 28+16)) {
    printf("DECRYPT SUCCES: \n");
    dump(hash, 28);
  } else {
    printf("DECRYPT FAILED.\n");
  }
  quic_crypto_free_key(k);

  s.hash = TLS_hash_SHA256;
  s.ae = TLS_aead_CHACHA20_POLY1305;

  if(!quic_crypto_derive_key(&k, &s))
  {
    printf("Failed to derive key\n");
    exit(1);
  }

  printf("\nCHACHA20-POLY1305 encrypt test:\n");
  quic_crypto_encrypt(k, cipher, 0x29e255a7, salt, 13, data, 28);
  dump(cipher, 28+16);

  if(quic_crypto_decrypt(k, hash, 0x29e255a7, salt, 13, cipher, 28+16)) {
    printf("DECRYPT SUCCES: \n");
    dump(hash, 28);
  } else {
    printf("DECRYPT FAILED.\n");
  }

  quic_crypto_free_key(k);
  printf("==== PASS:  coverage ====\n\n\n");
}

//////////////////////////////////////////////////////////////////////////////


const uint64_t sn = 0x29;

#define ad_len      0x11
const char ad[ad_len] =
{0xff,0x00,0x00,0x4f,0xee,0x00,0x00,0x42,0x37,0xff,0x00,0x00,0x09,0x00,0x00,0x00};

#define plain_len 256
const char plain[plain_len] = {
0x12,0x00,0x40,0xda,0x16,0x03,0x03,0x00,0xd5,0x01,0x00,0x00,0xd1,0x03,0x03,0x5a,
0xa0,0x4e,0xf1,0x3b,0x74,0x7c,0x34,0xe6,0x74,0x05,0xc9,0x1f,0x0a,0x2a,0x5c,0x5d,
0x1f,0xf9,0x08,0x01,0x77,0xb5,0xe8,0x35,0x94,0x34,0x70,0xbd,0xdd,0x86,0xa5,0x00,
0x00,0x04,0x13,0x03,0x13,0x01,0x01,0x00,0x00,0xa4,0x00,0x1a,0x00,0x2c,0xff,0x00,
0x00,0x09,0x00,0x26,0x00,0x01,0x00,0x04,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x04,
0x00,0x04,0x00,0x00,0x00,0x02,0x00,0x04,0x00,0x00,0x00,0x21,0x00,0x08,0x00,0x04,
0x00,0x00,0x00,0x23,0x00,0x03,0x00,0x02,0x00,0x64,0x00,0x2b,0x00,0x03,0x02,0x7f,
0x17,0x00,0x33,0x00,0x26,0x00,0x24,0x00,0x1d,0x00,0x20,0x33,0xc5,0x86,0xee,0x6c
};

const char cipher_sha256_aes128[] = {
0x25,0x46,0xfe,0x2b,0x6c,0x86,0xf0,0x01,0x98,0x66,0x31,0x05,0xc8,0xa8,0x5c,0xec,
0x5f,0x5f,0xbf,0x17,0x10,0x55,0x96,0x49,0xfa,0x28,0x01,0x89,0x96,0x9a,0xcb,0x6b,
0x3b,0x65,0xec,0xaa,0x96,0x6b,0xb5,0x08,0x18,0x8f,0x05,0x29,0x03,0x9a,0x87,0x71,
0x9a,0xb9,0x5b,0x42,0x17,0xd5,0x04,0x4f,0x44,0x33,0x6a,0x1f,0x91,0x07,0xcb,0xe4,
0xdb,0x96,0x3c,0xae,0x3e,0x8c,0x34,0x0c,0xcb,0xd6,0xfd,0xa1,0x52,0x2e,0xb7,0x47,
0x22,0x52,0x68,0xae,0x99,0x0c,0x63,0x85,0x67,0x09,0x7c,0x9c,0x38,0xe6,0x9c,0x29,
0x7d,0xff,0x8f,0xe5,0xa8,0x9f,0xf1,0xed,0x06,0x6f,0x92,0x50,0x7a,0x13,0x2f,0xa9,
0x13,0x8e,0x6c,0xe9,0x1f,0x88,0x74,0xf7,0xba,0x2f,0x33,0x50,0x5d,0x55,0x1d,0xd1,
0x14,0x24,0x9b,0xf5,0x41,0x28,0x27,0x9b,0x53,0x04,0xbe,0x55,0x9e,0x7b,0x34,0x5b,
0x4c,0x29,0x7c,0xa8,0x25,0x5d,0x49,0x7e,0x75,0xa8,0xb6,0x71,0x24,0x00,0x20,0xc9,
0x78,0xeb,0x37,0x48,0x8f,0x33,0x41,0x88,0xe3,0x85,0xa8,0x2a,0x3c,0x5e,0x05,0x86,
0x7e,0x36,0xd0,0x84,0x96,0x0e,0xce,0x7f,0x07,0xcc,0x97,0x8f,0x5a,0xae,0x3b,0xac,
0x08,0xab,0x63,0x53,0x33,0x37,0x6a,0x15,0x43,0xa9,0x38,0x18,0xc6,0x92,0xe5,0x25,
0xc8,0x6e,0xae,0x66,0xd6,0x52,0x11,0x5d,0x0a,0xb0,0x5e,0xaa,0x53,0x01,0x0b,0x78,
0xfd,0x26,0x2e,0x9a,0x1f,0x5e,0xb4,0x65,0x3e,0xd2,0x4c,0x20,0xba,0x55,0x3f,0xf6,
0x0b,0xa1,0xd4,0x25,0x88,0xa2,0xc3,0xda,0xa4,0xc8,0x2c,0x4d,0x69,0x61,0xcd,0xae,
0x83,0x28,0x91,0x23,0x50,0x19,0xc6,0xf9,0x2b,0xbc,0xe1,0x36,0x61,0x16,0xb5,0x45,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

};

const char cipher_sha384_aes128[] = {
0x5c,0x4a,0x57,0x24,0x93,0x30,0x1b,0xbb,0x9c,0x66,0x8a,0xe0,0xfb,0xdc,0x13,0xc2,
0xba,0xfe,0xd3,0xfe,0xa6,0x9a,0x40,0x2c,0xb3,0x73,0x8c,0x88,0x81,0xae,0x4a,0x3e,
0x2c,0x7e,0xe0,0xaa,0x25,0xfc,0x97,0xf7,0xdb,0xdd,0x45,0xdc,0x16,0xe7,0x7c,0x9a,
0xb1,0x40,0x4c,0xe8,0xcb,0xaa,0x78,0x3a,0x19,0xd0,0xe8,0xce,0x6d,0x8a,0x67,0xea,
0x86,0x3c,0x6c,0xac,0x31,0x24,0x8e,0x3a,0x8c,0xcf,0xf4,0xc7,0x12,0x1b,0xaa,0xfd,
0x56,0x04,0x7d,0xeb,0x2b,0xf4,0x1c,0x21,0xff,0xdf,0x6b,0x31,0x5c,0x44,0xe5,0x40,
0xcf,0x07,0x8c,0x99,0xdf,0xaf,0xe8,0xb3,0x83,0xe8,0x02,0x0a,0x10,0xe0,0x09,0x6f,
0xde,0xc8,0x4c,0x3c,0xb2,0x00,0x75,0x35,0x02,0x0c,0xce,0xb0,0x6c,0x4f,0xbd,0x33,
0xed,0x27,0x53,0x02,0xff,0xae,0xa1,0x89,0x35,0x36,0x27,0x78,0x4f,0xeb,0x34,0x0d,
0xe5,0x0b,0xbd,0x43,0x79,0x47,0xf2,0xca,0x7e,0xc9,0x11,0x81,0x60,0xcf,0xa5,0xea,
0x53,0xa6,0x33,0x40,0xe9,0x85,0x14,0x41,0x66,0xd0,0x04,0x47,0xec,0x76,0x35,0x1c,
0x24,0x73,0xd5,0x8c,0x1d,0x83,0x0b,0x20,0x6b,0xfa,0x16,0x13,0x13,0x3f,0xf8,0xaa,
0xc5,0xde,0xc9,0xf0,0x2b,0x81,0xea,0x66,0xf2,0xf5,0xee,0x81,0xf5,0x79,0xd0,0x8e,
0x44,0x7b,0xb3,0x57,0x76,0x3f,0x63,0xc7,0x63,0x81,0x9a,0x11,0x32,0x73,0xc6,0xca,
0x4c,0xc4,0x9b,0x7f,0x0d,0xd0,0xb0,0xfe,0x5b,0x7e,0x59,0xd4,0xb2,0x92,0x35,0xcc,
0xb5,0xe5,0xb9,0xb0,0x6b,0x63,0xb1,0xa1,0x69,0xa9,0x17,0x8a,0xb5,0x13,0x3c,0xde,
0x7f,0x56,0xc3,0x9b,0x3b,0xf6,0x79,0xc2,0x3e,0x45,0xa8,0x5e,0x38,0xcd,0x3c,0x90,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

};

const char cipher_sha512_aes128[] = {
0xb9,0x6f,0x4d,0x24,0x08,0x54,0x65,0x4f,0x46,0xb4,0xd7,0x69,0x77,0x67,0x18,0xbe,
0x37,0x97,0xff,0x08,0x68,0x97,0x2c,0xe0,0x9e,0x66,0x18,0x8e,0x81,0x53,0xa6,0xa5,
0x1a,0x63,0x59,0x3e,0x5e,0x28,0x47,0xc0,0x50,0xa2,0xfc,0xba,0xca,0xbc,0xa3,0x14,
0x18,0xb5,0x8b,0x55,0xb6,0xa1,0x74,0x82,0x83,0x46,0xea,0x7c,0x8a,0x3a,0x14,0x08,
0x57,0xe1,0xf6,0xed,0xf8,0xe4,0xe1,0xc3,0xc2,0x92,0xaf,0x12,0x29,0x72,0xcd,0xc3,
0x30,0x6f,0x11,0xa5,0x43,0xd2,0x69,0x23,0xb8,0x55,0xef,0xeb,0xb3,0x9c,0xee,0xb0,
0x03,0xee,0xe1,0x8d,0x6c,0x75,0x11,0x88,0x8a,0xb6,0x5e,0x35,0x12,0x11,0xc4,0xe9,
0xa2,0xa6,0xeb,0xa1,0xbd,0x45,0xb8,0x6c,0x09,0x03,0xa4,0x54,0x9b,0x35,0xff,0x98,
0xf1,0xbd,0xca,0xef,0x7b,0x8b,0x63,0xe1,0x91,0xa1,0xb5,0x23,0xd1,0x1b,0xd7,0x07,
0xe6,0x16,0xa0,0xe2,0x5d,0xf2,0x94,0x1f,0xa3,0x5c,0x71,0x12,0x66,0x6d,0x27,0x43,
0xe3,0xf6,0x31,0x28,0x73,0x0f,0x20,0xd0,0x28,0xce,0x22,0x40,0x62,0xa0,0x2a,0x54,
0x2c,0x3e,0xbc,0x60,0x09,0xe2,0x51,0xe4,0x3f,0x91,0xec,0x17,0x45,0xa8,0x2e,0x7e,
0x33,0x1f,0x3e,0xf1,0x4f,0xbd,0x4f,0x37,0xb7,0x62,0xe5,0xb8,0xe7,0x7b,0x4f,0x09,
0x7f,0x30,0xfa,0x6c,0x5d,0xbf,0xac,0xf7,0x10,0xd9,0xf6,0xfa,0x44,0x5f,0xf5,0x28,
0x4e,0x61,0x07,0x52,0xdc,0x07,0xab,0x25,0x10,0x79,0xeb,0x22,0xed,0x70,0x92,0x67,
0x1e,0x1a,0xe9,0x40,0x61,0x66,0x2b,0x6e,0x04,0x94,0x3e,0xeb,0xd9,0x46,0x3c,0x4e,
0x8e,0xc3,0xe7,0xa2,0x3c,0x55,0x8b,0x15,0xfd,0xe7,0xb0,0xc7,0x6a,0xbf,0xdc,0x71,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

};

const char cipher_sha256_aes256[] = {
0x46,0x6a,0x34,0xee,0xf4,0xad,0xba,0xf5,0xf0,0xe7,0x7c,0xe8,0xd6,0x6c,0x6b,0xaf,
0x34,0x7e,0xc2,0x7e,0x15,0x5d,0x01,0xb6,0x1f,0x43,0xc4,0xf5,0x17,0x39,0x50,0x7b,
0xea,0xfb,0xdb,0x68,0x62,0xee,0x02,0x21,0x05,0xea,0x0d,0x3d,0x58,0x52,0xe6,0xe4,
0xae,0xfe,0x8d,0x31,0x32,0xd3,0x29,0x8c,0xe2,0x4c,0x26,0x84,0x9f,0x74,0xab,0x21,
0x6a,0xb0,0x36,0x99,0x24,0x9b,0x74,0x8c,0x76,0x01,0x37,0x11,0x35,0x19,0x85,0x94,
0x19,0x68,0xe8,0xa8,0x10,0xc6,0xd7,0xa3,0x38,0x9e,0x8c,0xc9,0xa3,0xdf,0x15,0x99,
0xf6,0x0c,0xf0,0x37,0xbd,0xe0,0x71,0x7e,0xfe,0x61,0x43,0x41,0x81,0x4d,0x90,0x35,
0x95,0x2e,0x42,0x3f,0xa8,0x8a,0xe4,0xc4,0x5a,0x35,0x20,0x46,0x08,0x7b,0xf3,0x5a,
0xb2,0x4c,0x75,0x84,0x36,0xc6,0xde,0x89,0x84,0xb1,0x29,0x6f,0xf7,0xa9,0x40,0x5e,
0xe3,0x31,0xb3,0x78,0x6e,0xc6,0xb7,0x28,0x49,0xa7,0x0d,0x3b,0xf7,0xbc,0xeb,0x5b,
0xd3,0xbb,0x3d,0xb2,0xa6,0x47,0x95,0x55,0xdb,0xfa,0x78,0x4f,0x83,0x73,0x5e,0x8d,
0x51,0x5d,0xa7,0x47,0x92,0x1e,0x33,0x7a,0x7f,0x0a,0xcf,0x05,0x59,0x74,0x0a,0xb4,
0x59,0x11,0x55,0x76,0x6a,0xd7,0x61,0x62,0x14,0x04,0x1e,0xef,0x52,0x8b,0x46,0xd8,
0x50,0xa8,0x5a,0x51,0xd4,0xec,0x20,0xbe,0x35,0x01,0xfe,0xd9,0x86,0x73,0xbd,0x53,
0x94,0x05,0x82,0x65,0xa3,0x80,0xac,0x9e,0x3d,0xfc,0x0e,0xf7,0xee,0x5d,0x12,0xb3,
0xd1,0xa6,0x90,0xc3,0x35,0x8b,0x05,0x45,0x88,0xbb,0x38,0x4b,0xd2,0x38,0xa5,0xbf,
0x9e,0x16,0x59,0x2a,0x20,0xa3,0x9d,0x3d,0x2c,0x87,0xd4,0x28,0xce,0xab,0xb9,0x1c,
};

const char cipher_sha384_aes256[] = {
0x90,0xe7,0x2b,0xa9,0xf5,0x37,0xe7,0x2c,0xb3,0x46,0x5f,0x8d,0xb8,0xda,0xae,0x7a,
0x0b,0x66,0x63,0xd4,0x43,0x94,0x32,0xe0,0xa6,0x6f,0xde,0x8b,0x09,0x9b,0x62,0x92,
0x45,0xbf,0x73,0xac,0x6b,0xc7,0xab,0x8b,0x5c,0x50,0x9a,0xfd,0x47,0xb7,0xd6,0x29,
0xc6,0x9a,0x56,0xe2,0xd8,0x50,0x86,0x27,0x67,0xce,0xab,0x01,0xf4,0xe7,0xe7,0xd1,
0x76,0xb0,0x38,0xcd,0xbe,0x9a,0x66,0x6d,0x5a,0x6e,0xaf,0x87,0x0a,0x7f,0xdb,0x98,
0x46,0x0d,0xcf,0xd5,0x81,0x2a,0x1d,0x60,0x22,0x81,0xa4,0x19,0xc1,0x0f,0x13,0x9c,
0xe5,0x2c,0x6a,0xe5,0x46,0xd8,0x6f,0x77,0xe8,0xa0,0xab,0xea,0x22,0x43,0x10,0x9f,
0xc2,0xee,0x6b,0x9f,0x26,0x53,0x82,0xc8,0xb6,0x0a,0xe0,0x55,0xe4,0x07,0xc3,0x7b,
0x31,0x05,0x5f,0x75,0x9a,0x2f,0xf7,0xe1,0x2a,0x5b,0x34,0x4c,0x9f,0x53,0x7f,0xc2,
0x97,0x25,0x51,0xf3,0xc8,0x56,0x56,0x23,0x8e,0x32,0x95,0x1e,0xcc,0x33,0x55,0x57,
0x6b,0x65,0xca,0xbe,0xd0,0xba,0xb5,0x9c,0x91,0x43,0x9c,0x30,0x17,0x0f,0x20,0xf4,
0x0a,0x1e,0xf0,0xe4,0xde,0x0c,0x47,0x0e,0x3f,0x5d,0x4e,0x9b,0x2b,0x09,0x7c,0x18,
0xab,0x32,0xa4,0x27,0x2f,0xba,0x10,0xea,0x0e,0x02,0x96,0xbb,0x77,0x00,0x60,0x99,
0xe8,0xff,0xaf,0x94,0x7c,0x46,0x13,0x97,0x3a,0xf0,0xd3,0x88,0x8d,0xef,0x86,0xb7,
0x3a,0x87,0xf8,0x57,0xc5,0x26,0x66,0x06,0xf9,0xf8,0xc5,0x34,0xda,0x95,0x89,0x1c,
0x2e,0x33,0x56,0x11,0x5b,0xea,0x72,0xc6,0x4d,0x37,0x74,0x40,0x7c,0xbc,0xbf,0x33,
0xb4,0x13,0x35,0x92,0xa8,0x95,0x0b,0x68,0x13,0xf9,0xdf,0x85,0x96,0xd8,0xba,0xbf,
};

const char cipher_sha512_aes256[] = {
0xb7,0x9d,0xdf,0xfb,0x62,0x1c,0xd6,0xef,0xf3,0x79,0xb4,0xdd,0x34,0xb8,0x71,0x02,
0x15,0x66,0x4a,0x8f,0x9b,0x0b,0xe0,0x6d,0x36,0x8a,0xe5,0xc6,0x72,0x5f,0xd8,0x00,
0x31,0x4b,0x5b,0xc2,0xf2,0xbc,0xe9,0xb2,0xbb,0x0d,0x0e,0xec,0x0f,0x1d,0x0f,0xa6,
0xad,0x4a,0x0b,0x90,0x42,0x1d,0xa0,0x1e,0xa6,0xc3,0x76,0x87,0x28,0x86,0x15,0x1f,
0xbc,0xc7,0xa4,0xdd,0x3f,0x14,0x63,0xcd,0xef,0x96,0xd4,0xdd,0x32,0x4a,0x7d,0x35,
0x1f,0x43,0x65,0x1f,0xd1,0xe2,0xaf,0xc9,0x2f,0x22,0xd7,0x58,0x1e,0xfc,0x78,0x42,
0x5e,0x54,0x8a,0x96,0xaa,0xff,0x34,0x0d,0xf6,0x59,0x21,0x62,0xdd,0x26,0x18,0xa3,
0x10,0x01,0x92,0x64,0xec,0x90,0x3c,0x87,0x76,0x28,0x36,0x2a,0x69,0xef,0x74,0x62,
0x42,0x51,0x8c,0xf3,0x77,0x7f,0xcd,0x85,0x08,0x8a,0x9b,0x5b,0xd4,0xb1,0x63,0xf2,
0xbf,0xd8,0x3e,0x78,0xaa,0xc8,0x12,0xbc,0x6b,0xc8,0x65,0x15,0xb0,0x75,0x42,0xb0,
0x2a,0xda,0xaf,0xa5,0xc4,0x7a,0xf7,0xdd,0xec,0xf8,0x66,0xc9,0xa4,0x44,0x64,0x78,
0x64,0x50,0xab,0x5a,0x82,0xe1,0x73,0xb1,0x51,0xc6,0x7d,0xfd,0x36,0xb6,0x13,0x9e,
0x40,0xf3,0xca,0xe8,0x2a,0xea,0x7b,0xfb,0xa1,0xd4,0xc3,0x30,0x89,0xba,0x6a,0xad,
0x3d,0x83,0x04,0xa2,0xc5,0x4c,0x5f,0x2a,0xe5,0xd2,0x3a,0x72,0x0a,0x5b,0xb4,0x85,
0xd1,0x95,0x73,0x9a,0x59,0x8f,0x65,0x56,0xf3,0x9f,0x05,0xe8,0xe8,0x2c,0x3a,0x6e,
0x0f,0x2c,0x30,0xe8,0x92,0xd9,0x93,0xcd,0x02,0xbe,0xda,0xb6,0x81,0xea,0x5c,0x22,
0x5d,0xa2,0xea,0xed,0x8a,0x22,0x23,0x97,0xae,0x6b,0xee,0xe9,0xe1,0x34,0x8d,0xb2,
};

const char cipher_sha256_chachapoly[] = {
0xcd,0xae,0x6e,0x34,0xf8,0x16,0x90,0xbe,0x3e,0xd7,0x9c,0xdd,0xab,0x2e,0xbb,0xc0,
0xd9,0xc8,0xb1,0xd0,0x76,0x29,0x85,0xa4,0x3e,0x85,0x4f,0xbf,0xa0,0x64,0x4e,0x1e,
0x31,0x35,0xe4,0xc1,0x67,0xec,0x1a,0xc0,0x42,0xf5,0x65,0x75,0xee,0xf5,0x2e,0x6a,
0x79,0x76,0xe4,0x4d,0xae,0x31,0xcf,0x46,0x34,0xcd,0x48,0x92,0xd3,0x95,0x28,0x32,
0x01,0xa4,0x60,0x8b,0xb0,0xe7,0x14,0x52,0x36,0x41,0x03,0x2c,0x6b,0x91,0xa1,0x69,
0x34,0x21,0xf6,0x13,0xa2,0x06,0x4c,0x06,0xbf,0x46,0x52,0xe1,0xe0,0xf5,0xfa,0x4b,
0xc7,0x19,0x2f,0x77,0x03,0x84,0xe7,0x0f,0x7a,0x66,0x3d,0x05,0x80,0xd6,0xb3,0x22,
0xf8,0x80,0x26,0xe6,0x80,0xa3,0xc2,0xd0,0x1c,0x3f,0xcc,0xa0,0x8c,0x50,0xfc,0xb8,
0x0f,0x2a,0xf1,0x2d,0x78,0x65,0x4b,0x6a,0x7b,0xa9,0x7d,0x20,0xc5,0xbb,0xbf,0x29,
0x7f,0xe3,0xac,0xed,0xae,0x50,0xd9,0x8e,0x92,0x13,0x2f,0xd3,0x2a,0x59,0x93,0x4b,
0x2f,0x40,0xdb,0x3b,0xc6,0x16,0xa5,0x9e,0x42,0x89,0x48,0xf7,0xd6,0xc8,0x3a,0xe2,
0x18,0x1f,0x84,0xc4,0xab,0x70,0xe8,0x1c,0x22,0xbf,0x69,0x11,0x81,0x70,0xde,0xd8,
0x94,0x77,0xc9,0x5c,0x90,0x7b,0x9c,0x86,0x84,0xac,0x96,0xf3,0x02,0x22,0x3b,0x85,
0x80,0xb3,0xd7,0x74,0x4e,0x40,0xc4,0x04,0x3a,0x5d,0xaf,0x80,0x8c,0x79,0x09,0xe3,
0x7d,0x53,0xea,0xb3,0x1f,0xf8,0xf8,0x42,0xaf,0x7b,0xb4,0x33,0xe1,0x8f,0xa1,0x97,
0x01,0x8b,0x30,0x27,0x4b,0xcb,0xbc,0x0b,0x02,0x8e,0x04,0x9c,0xd7,0x54,0x66,0x17,
0xb9,0x07,0x32,0x60,0xc2,0x08,0x9d,0xd1,0x63,0xfa,0x58,0x65,0xb6,0x26,0x09,0xbf,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const char cipher_sha384_chachapoly[] = {
0x77,0x78,0x87,0x2e,0x38,0xcd,0x71,0xfc,0xb9,0x7a,0xcf,0x54,0x88,0x74,0x5f,0x20,
0x43,0x2a,0x8a,0x94,0x1b,0xf0,0x72,0xad,0x9f,0x39,0x15,0x56,0x0e,0x04,0x97,0x15,
0xdc,0x81,0xf4,0x1c,0x0a,0xb7,0x58,0xc0,0xf9,0x4f,0x2a,0x48,0xcb,0xed,0x54,0x73,
0xb0,0xa1,0xf0,0x4f,0x8f,0xb0,0x1d,0x86,0x4b,0x6a,0xee,0x44,0x8d,0x98,0x76,0xe6,
0xf5,0xba,0xd3,0x79,0xcb,0x06,0xe3,0x7f,0x01,0xb1,0x1a,0x1d,0x79,0x37,0x46,0x9d,
0x55,0x32,0x68,0xbe,0x9d,0xb4,0x70,0xf5,0x48,0xda,0xb1,0x91,0xcd,0xcd,0xf5,0x23,
0xaf,0xf5,0xb6,0xdd,0x05,0x82,0x79,0x2b,0x15,0xe7,0x52,0x38,0x77,0x6d,0x66,0x22,
0xf8,0x62,0xf5,0x4b,0xeb,0x1a,0xbf,0x5f,0x70,0x7a,0x8b,0xd6,0x97,0x47,0xc3,0x22,
0x6e,0x91,0xc4,0x3a,0x2f,0x83,0x43,0xae,0x7f,0xa3,0xa3,0x64,0xd9,0x5b,0x81,0x6b,
0xa6,0xd6,0xb5,0x23,0x99,0x8d,0x5b,0xf8,0xc9,0x65,0x7a,0x69,0x38,0x5c,0x4d,0xb5,
0xfb,0xee,0xe9,0x56,0xed,0xcd,0x35,0x11,0xd2,0x65,0x2f,0xd2,0x88,0x27,0xbc,0x26,
0x7b,0x5d,0x2d,0x4f,0x4a,0x01,0x7d,0x21,0xe5,0x24,0x9e,0xa5,0xcb,0xf6,0x4a,0x04,
0x94,0xfe,0x13,0x95,0x6b,0x4d,0xeb,0x55,0x05,0x50,0xc4,0xee,0x8e,0xd8,0xfc,0x57,
0x86,0xfa,0x5e,0x3f,0x78,0x15,0x29,0xf7,0x7c,0xe5,0x3e,0xd5,0x0a,0x1e,0x70,0x91,
0x39,0x19,0x80,0xb5,0x70,0xf5,0x25,0x7e,0xe0,0xa1,0x73,0x8b,0x88,0x9e,0x40,0x8a,
0x7a,0x06,0x9c,0xeb,0x56,0xfe,0x6c,0x77,0xf6,0xc5,0x16,0x0c,0xaf,0x54,0x60,0xff,
0x19,0x84,0xb4,0xc9,0x94,0x50,0x81,0x43,0x6b,0xbe,0x04,0xa0,0x76,0x2f,0x9a,0xf5,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const char cipher_sha512_chachapoly[] = {
0x7a,0xf8,0xff,0xc7,0xad,0x8b,0xfb,0x26,0xd9,0xaf,0xf8,0x24,0xf9,0x94,0xe9,0xee,
0x6e,0x4b,0xbb,0x45,0x1d,0x51,0x57,0x0a,0x43,0x77,0x31,0xe6,0xda,0xbc,0xaa,0xc6,
0x4d,0x30,0x3a,0x45,0x71,0x1a,0x2c,0xd2,0xad,0x58,0x20,0x9a,0xca,0xb6,0xa3,0xc7,
0x9d,0x8d,0x4d,0x99,0x3e,0x3b,0x8c,0x9b,0xb6,0x35,0x43,0x51,0x0d,0x21,0xdd,0x46,
0x41,0x5a,0x86,0x12,0xdc,0xeb,0x3a,0x5c,0xaa,0x97,0xea,0x9e,0xe8,0xc7,0xbe,0x92,
0xd4,0x03,0xdc,0x21,0x54,0x88,0x1d,0xce,0x40,0x68,0x79,0xea,0x88,0x92,0x63,0x31,
0x60,0x4c,0x75,0xe3,0xbf,0xa2,0x15,0x6c,0x84,0xb3,0xfa,0xb1,0x0a,0x94,0xe8,0x2c,
0xa4,0xb1,0x21,0xed,0xc2,0x37,0xa7,0xa3,0xc4,0xb5,0xc3,0xd2,0xf5,0x2e,0xcd,0x03,
0x4e,0xfd,0x73,0x90,0xe9,0x0b,0x48,0xc3,0xce,0x28,0x3c,0x8e,0xcd,0x46,0xa3,0x2d,
0x2d,0x3e,0x21,0x6e,0xf9,0xd3,0xc9,0x7d,0x98,0x31,0x21,0xd7,0x5d,0xd1,0x40,0xfd,
0xc5,0x5b,0xb1,0xda,0x61,0x49,0xb0,0x12,0xe6,0x6a,0xaf,0xc2,0x42,0x12,0xa8,0x05,
0xe8,0x8f,0x97,0xf7,0xef,0x37,0x6e,0x90,0x1a,0xcc,0x85,0xbd,0x0e,0x03,0x04,0xc8,
0xa6,0xb6,0x63,0x3f,0xf7,0xf9,0x34,0xa3,0xcf,0xb6,0xa1,0xc2,0xe0,0x3c,0x82,0x9f,
0x73,0xab,0x47,0x20,0x63,0x5d,0x3f,0xaa,0x28,0xb5,0x60,0x13,0xc2,0xec,0xf7,0x71,
0x80,0xd3,0xc6,0x50,0x8b,0x9e,0xe5,0xce,0x62,0xe7,0xa5,0xbd,0xec,0xef,0x45,0xbc,
0x9c,0xe4,0x11,0x22,0xa0,0xfc,0xac,0xe5,0xd3,0x17,0x1a,0x2f,0x66,0xec,0x1a,0x39,
0x2e,0x2b,0xb1,0xe3,0x98,0x06,0xf2,0xaf,0xcb,0x91,0xb4,0x9b,0x17,0x78,0x63,0xda,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

struct testcombination {
    mitls_hash hash;
    mitls_aead ae;
    const char *expected_cipher;
} testcombinations[] = {
    // TLS_hash_MD5 and TLS_hash_SHA1 aren't supported by libquiccrypto

    { TLS_hash_SHA256, TLS_aead_AES_128_GCM, cipher_sha256_aes128 },
    { TLS_hash_SHA384, TLS_aead_AES_128_GCM, cipher_sha384_aes128 },
    { TLS_hash_SHA512, TLS_aead_AES_128_GCM, cipher_sha512_aes128 },

    { TLS_hash_SHA256, TLS_aead_AES_256_GCM, cipher_sha256_aes256 },
    { TLS_hash_SHA384, TLS_aead_AES_256_GCM, cipher_sha384_aes256 },
    { TLS_hash_SHA512, TLS_aead_AES_256_GCM, cipher_sha512_aes256 },

    { TLS_hash_SHA256, TLS_aead_CHACHA20_POLY1305, cipher_sha256_chachapoly },
    { TLS_hash_SHA384, TLS_aead_CHACHA20_POLY1305, cipher_sha384_chachapoly },
    { TLS_hash_SHA512, TLS_aead_CHACHA20_POLY1305, cipher_sha512_chachapoly },

    { 0, 0, NULL }
};

// map mitls_hash to a string name
const char *hash_to_name[] = {
    "TLS_hash_MD5",
    "TLS_hash_SHA1",
    "TLS_hash_SHA224",
    "TLS_hash_SHA256",
    "TLS_hash_SHA384",
    "TLS_hash_SHA512"
};

// map mitls_aead to a string name
const char *aead_to_name[] = {
    "TLS_aead_AES_128_GCM",
    "TLS_aead_AES_256_GCM",
    "TLS_aead_CHACHA20_POLY1305"
};

void dumptofile(FILE *fp, const char buffer[], size_t len)
{
  size_t i;
  for(i=0; i<len; i++) {
    fprintf(fp, "0x%2.2x,", (unsigned char)buffer[i]);
    if (i % 16 == 15 || i == len-1) fprintf(fp, "\n");
  }
}

void check_result(const char* testname, const char *actual, const char *expected, uint32_t length)
{
    for (uint32_t i=0; i<length; ++i) {
        if (actual[i] != expected[i]) {
            printf("FAIL %s:  actual 0x%2.2x mismatch with expected 0x%2.2x at offset %u\n",
              testname, actual[i], expected[i], i);
            exit(1);
        }
    }
}

void test_crypto(const quic_secret *secret, const char *expected_cipher)
{
    int result;
    quic_key *key;
    char cipher[plain_len+16];

    printf("==== test_crypto(%s,%s) ====\n",
        hash_to_name[secret->hash], aead_to_name[secret->ae]);

    result = quic_crypto_derive_key(&key, secret);
    if (result == 0) {
        printf("FAIL: quic_crypto_derive_key failed\n");
        exit(1);
    }

    memset(cipher, 0, sizeof(cipher));
    result = quic_crypto_encrypt(key, cipher, sn, ad, ad_len, plain, plain_len);
    if (result == 0) {
        printf("FAIL: quic_crypto_encrypt failed\n");
        exit(1);
    }

#if 0
    // Capture expected results to files ready to paste into C source code
    char fname[64];
    sprintf(fname, "results_%d_%d", secret->hash, secret->ae);
    FILE *fp = fopen(fname, "w");
    dumptofile(fp, cipher, sizeof(cipher));
    fclose(fp);
#else
    // Verify that the computed text matches expectations
    check_result("quic_crypto_encrypt", cipher, expected_cipher, sizeof(cipher));
#endif

    char decrypted[plain_len];
    result = quic_crypto_decrypt(key, decrypted, sn, ad, ad_len, cipher, sizeof(cipher));
    if (result == 0) {
        printf("FAIL: quic_crypto_decrypt failed\n");
        exit(1);
    }
    check_result("quic_crypto_decrypt", decrypted, plain, sizeof(decrypted));

    result = quic_crypto_free_key(key);
    if (result == 0) {
        printf("FAIL: quic_crypto_free_key failed\n");
        exit(1);
    }

    printf("PASS\n");
}

const uint8_t expected_client_hs[] = {
0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0xf5,0x18,0x32,0x15,0xff,0x27,0x27,
0x14,0xa8,0x16,0xd7,0xfb,0x56,0x63,0x3e,0x7d,0x44,0xbe,0x78,0xea,0xbf,0x9b,0x51,
0xb1,0x33,0xc9,0x59,0x1a,0xa7,0x38,0x4d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t expected_server_hs[] = {
0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x62,0x8c,0x59,0x00,0x3e,0x00,0x33,
0xed,0xa3,0x7f,0x0a,0xb3,0x42,0x0d,0xe9,0x22,0x06,0x61,0xfb,0x46,0x24,0x55,0x99,
0xb7,0xfc,0xc6,0x0e,0x9d,0xfb,0xa7,0xdd,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

void test_handshake_secrets(void)
{
    int result;

    printf("==== test_handshake_secrets() ====\n");

    quic_secret client_hs;
    memset(&client_hs, 0, sizeof(client_hs));

    quic_secret server_hs;
    memset(&server_hs, 0, sizeof(server_hs));

    const char con_id[12] = {0xff,0xaa,0x55,0x00, 0x80,0x01,0x7f,0xee, 0x81,0x42,0x24,0x18 };
    const char salt[] = {0xaf,0xc8,0x24,0xec,0x5f,0xc7,0x7e,0xca,0x1e,0x9d,0x36,0xf3,0x7f,0xb2,0xd4,0x65,0x18,0xc3,0x66,0x39};
    result = quic_derive_handshake_secrets(&client_hs, &server_hs, con_id, sizeof(con_id), salt, sizeof(salt));
    if (result == 0) {
        printf("FAIL: quic_derive_handshake_secrets failed\n");
        exit(1);
    }

#if 0
    // Capture expected results to files ready to paste into C source code
    FILE *fp = fopen("client_hs", "w");
    dumptofile(fp, (const uint8_t*)&client_hs, sizeof(client_hs));
    fclose(fp);
    fp = fopen("server_hs", "w");
    dumptofile(fp, (const uint8_t*)&server_hs, sizeof(server_hs));
    fclose(fp);
#else
    // Verify that the computed text matches expectations
    check_result("quic_derive_handshake_secrets client", (const char*)&client_hs, (const char*)&expected_client_hs, sizeof(client_hs));
    check_result("quic_derive_handshake_secrets server", (const char*)&server_hs, (const char*)&expected_server_hs, sizeof(server_hs));
#endif

    printf("==== PASS: test_handshake_secrets ==== \n");
}

void exhaustive(void)
{
    quic_secret secret;

    for (unsigned char i=0; i<sizeof(secret.secret); ++i) {
        secret.secret[i] = i;
    }

    for (size_t i=0; testcombinations[i].expected_cipher; ++i) {
        secret.hash = testcombinations[i].hash;
        secret.ae = testcombinations[i].ae;

        test_crypto(&secret, testcombinations[i].expected_cipher);
    }

    test_handshake_secrets();
}

int main(int argc, char **argv)
{
    coverage();
    exhaustive();

    printf("==== ALL TESTS PASS ====\n");
}
