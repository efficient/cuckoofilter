// Pulled from lookup3.c by Bob Jenkins
#include "hashutil.h"

#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#define mix(a,b,c)                              \
    {                                           \
        a -= c;  a ^= rot(c, 4);  c += b;       \
        b -= a;  b ^= rot(a, 6);  a += c;       \
        c -= b;  c ^= rot(b, 8);  b += a;       \
        a -= c;  a ^= rot(c,16);  c += b;       \
        b -= a;  b ^= rot(a,19);  a += c;       \
        c -= b;  c ^= rot(b, 4);  b += a;       \
    }

#define final(a,b,c)                            \
    {                                           \
        c ^= b; c -= rot(b,14);                 \
        a ^= c; a -= rot(c,11);                 \
        b ^= a; b -= rot(a,25);                 \
        c ^= b; c -= rot(b,16);                 \
        a ^= c; a -= rot(c,4);                  \
        b ^= a; b -= rot(a,14);                 \
        c ^= b; c -= rot(b,24);                 \
    }
// Assuming little endian
#define HASH_LITTLE_ENDIAN 1

#define get16bits(d) (*((const uint16_t *)(d)))

namespace cuckoofilter {
/*
  hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
  Returns a 32-bit value.  Every bit of the key affects every bit of
  the return value.  Two keys differing by one or two bits will have
  totally different hash values.

  The best hash table sizes are powers of 2.  There is no need to do
  mod a prime (mod is sooo slow!).  If you need less than 32 bits,
  use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
  In which case, the hash table should have hashsize(10) elements.

  If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

  By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
  code any way you wish, private, educational, or commercial.  It's free.

  Use for hash table lookup, or anything where one collision in 2^^32 is
  acceptable.  Do NOT use for cryptographic purposes.
*/

uint32_t HashUtil::BobHash(const std::string &s, uint32_t seed) {
  return BobHash(s.data(), s.length(), seed);
}

uint32_t HashUtil::BobHash(const void *buf, size_t length, uint32_t seed) {
  uint32_t a, b, c; /* internal state */
  union {
    const void *ptr;
    size_t i;
  } u; /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  // Is it safe to use key as the initial state setter?
  a = b = c = 0xdeadbeef + ((uint32_t)length) + seed;

  u.ptr = buf;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)buf; /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }

/*----------------------------- handle the last (probably partial) block */
/*
 * "k[2]&0xffffff" actually reads beyond the end of the string, but
 * then masks off the part it's not allowed to read.  Because the
 * string is aligned, the masked-off tail is in the same word as the
 * rest of the string.  Every machine with memory protection I've seen
 * does it on word boundaries, so is OK with this.  But VALGRIND will
 * still catch it and complain.  The masking trick does make the hash
 * noticably faster for short strings (like English words).
 */
#ifndef VALGRIND

    switch (length) {
      case 12:
        c += k[2];
        b += k[1];
        a += k[0];
        break;
      case 11:
        c += k[2] & 0xffffff;
        b += k[1];
        a += k[0];
        break;
      case 10:
        c += k[2] & 0xffff;
        b += k[1];
        a += k[0];
        break;
      case 9:
        c += k[2] & 0xff;
        b += k[1];
        a += k[0];
        break;
      case 8:
        b += k[1];
        a += k[0];
        break;
      case 7:
        b += k[1] & 0xffffff;
        a += k[0];
        break;
      case 6:
        b += k[1] & 0xffff;
        a += k[0];
        break;
      case 5:
        b += k[1] & 0xff;
        a += k[0];
        break;
      case 4:
        a += k[0];
        break;
      case 3:
        a += k[0] & 0xffffff;
        break;
      case 2:
        a += k[0] & 0xffff;
        break;
      case 1:
        a += k[0] & 0xff;
        break;
      case 0:
        return c; /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    const u_int8_t *k8;
    k8 = (const u_int8_t *)k;
    switch (length) {
      case 12:
        c += k[2];
        b += k[1];
        a += k[0];
        break;
      case 11:
        c += ((uint32_t)k8[10]) << 16; /* fall through */
      case 10:
        c += ((uint32_t)k8[9]) << 8; /* fall through */
      case 9:
        c += k8[8]; /* fall through */
      case 8:
        b += k[1];
        a += k[0];
        break;
      case 7:
        b += ((uint32_t)k8[6]) << 16; /* fall through */
      case 6:
        b += ((uint32_t)k8[5]) << 8; /* fall through */
      case 5:
        b += k8[4]; /* fall through */
      case 4:
        a += k[0];
        break;
      case 3:
        a += ((uint32_t)k8[2]) << 16; /* fall through */
      case 2:
        a += ((uint32_t)k8[1]) << 8; /* fall through */
      case 1:
        a += k8[0];
        break;
      case 0:
        return c;
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const u_int16_t *k = (const u_int16_t *)buf; /* read 16-bit chunks */
    const u_int8_t *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12) {
      a += k[0] + (((uint32_t)k[1]) << 16);
      b += k[2] + (((uint32_t)k[3]) << 16);
      c += k[4] + (((uint32_t)k[5]) << 16);
      mix(a, b, c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const u_int8_t *)k;
    switch (length) {
      case 12:
        c += k[4] + (((uint32_t)k[5]) << 16);
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 11:
        c += ((uint32_t)k8[10]) << 16; /* fall through */
      case 10:
        c += k[4];
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 9:
        c += k8[8]; /* fall through */
      case 8:
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 7:
        b += ((uint32_t)k8[6]) << 16; /* fall through */
      case 6:
        b += k[2];
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 5:
        b += k8[4]; /* fall through */
      case 4:
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 3:
        a += ((uint32_t)k8[2]) << 16; /* fall through */
      case 2:
        a += k[0];
        break;
      case 1:
        a += k8[0];
        break;
      case 0:
        return c; /* zero length requires no mixing */
    }

  } else { /* need to read the key one byte at a time */
    const u_int8_t *k = (const u_int8_t *)buf;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      a += ((uint32_t)k[1]) << 8;
      a += ((uint32_t)k[2]) << 16;
      a += ((uint32_t)k[3]) << 24;
      b += k[4];
      b += ((uint32_t)k[5]) << 8;
      b += ((uint32_t)k[6]) << 16;
      b += ((uint32_t)k[7]) << 24;
      c += k[8];
      c += ((uint32_t)k[9]) << 8;
      c += ((uint32_t)k[10]) << 16;
      c += ((uint32_t)k[11]) << 24;
      mix(a, b, c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length) /* all the case statements fall through */
    {
      case 12:
        c += ((uint32_t)k[11]) << 24;
      case 11:
        c += ((uint32_t)k[10]) << 16;
      case 10:
        c += ((uint32_t)k[9]) << 8;
      case 9:
        c += k[8];
      case 8:
        b += ((uint32_t)k[7]) << 24;
      case 7:
        b += ((uint32_t)k[6]) << 16;
      case 6:
        b += ((uint32_t)k[5]) << 8;
      case 5:
        b += k[4];
      case 4:
        a += ((uint32_t)k[3]) << 24;
      case 3:
        a += ((uint32_t)k[2]) << 16;
      case 2:
        a += ((uint32_t)k[1]) << 8;
      case 1:
        a += k[0];
        break;
      case 0:
        return c;
    }
  }

  final(a, b, c);
  return c;
}

/*
 * hashlittle2: return 2 32-bit hash values
 *
 * This is identical to hashlittle(), except it returns two 32-bit hash
 * values instead of just one.  This is good enough for hash table
 * lookup with 2^^64 buckets, or if you want a second hash if you're not
 * happy with the first, or if you want a probably-unique 64-bit ID for
 * the key.  *pc is better mixed than *pb, so use *pc first.  If you want
 * a 64-bit value do something like "*pc + (((uint64_t)*pb)<<32)".
 */
void HashUtil::BobHash(const void *buf, size_t length, uint32_t *idx1,
                       uint32_t *idx2) {
  uint32_t a, b, c; /* internal state */
  union {
    const void *ptr;
    size_t i;
  } u; /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32_t)length) + *idx1;
  c += *idx2;

  u.ptr = buf;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)buf; /* read 32-bit chunks */
#ifdef VALGRIND
    const uint8_t *k8;
#endif
    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }

/*----------------------------- handle the last (probably partial) block */
/*
 * "k[2]&0xffffff" actually reads beyond the end of the string, but
 * then masks off the part it's not allowed to read.  Because the
 * string is aligned, the masked-off tail is in the same word as the
 * rest of the string.  Every machine with memory protection I've seen
 * does it on word boundaries, so is OK with this.  But VALGRIND will
 * still catch it and complain.  The masking trick does make the hash
 * noticably faster for short strings (like English words).
 */
#ifndef VALGRIND

    switch (length) {
      case 12:
        c += k[2];
        b += k[1];
        a += k[0];
        break;
      case 11:
        c += k[2] & 0xffffff;
        b += k[1];
        a += k[0];
        break;
      case 10:
        c += k[2] & 0xffff;
        b += k[1];
        a += k[0];
        break;
      case 9:
        c += k[2] & 0xff;
        b += k[1];
        a += k[0];
        break;
      case 8:
        b += k[1];
        a += k[0];
        break;
      case 7:
        b += k[1] & 0xffffff;
        a += k[0];
        break;
      case 6:
        b += k[1] & 0xffff;
        a += k[0];
        break;
      case 5:
        b += k[1] & 0xff;
        a += k[0];
        break;
      case 4:
        a += k[0];
        break;
      case 3:
        a += k[0] & 0xffffff;
        break;
      case 2:
        a += k[0] & 0xffff;
        break;
      case 1:
        a += k[0] & 0xff;
        break;
      case 0:
        *idx1 = c;
        *idx2 = b;
        return; /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8_t *)k;
    switch (length) {
      case 12:
        c += k[2];
        b += k[1];
        a += k[0];
        break;
      case 11:
        c += ((uint32_t)k8[10]) << 16; /* fall through */
      case 10:
        c += ((uint32_t)k8[9]) << 8; /* fall through */
      case 9:
        c += k8[8]; /* fall through */
      case 8:
        b += k[1];
        a += k[0];
        break;
      case 7:
        b += ((uint32_t)k8[6]) << 16; /* fall through */
      case 6:
        b += ((uint32_t)k8[5]) << 8; /* fall through */
      case 5:
        b += k8[4]; /* fall through */
      case 4:
        a += k[0];
        break;
      case 3:
        a += ((uint32_t)k8[2]) << 16; /* fall through */
      case 2:
        a += ((uint32_t)k8[1]) << 8; /* fall through */
      case 1:
        a += k8[0];
        break;
      case 0:
        *idx1 = c;
        *idx2 = b;
        return; /* zero length strings require no mixing */
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16_t *k = (const uint16_t *)buf; /* read 16-bit chunks */
    const uint8_t *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12) {
      a += k[0] + (((uint32_t)k[1]) << 16);
      b += k[2] + (((uint32_t)k[3]) << 16);
      c += k[4] + (((uint32_t)k[5]) << 16);
      mix(a, b, c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8_t *)k;
    switch (length) {
      case 12:
        c += k[4] + (((uint32_t)k[5]) << 16);
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 11:
        c += ((uint32_t)k8[10]) << 16; /* fall through */
      case 10:
        c += k[4];
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 9:
        c += k8[8]; /* fall through */
      case 8:
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 7:
        b += ((uint32_t)k8[6]) << 16; /* fall through */
      case 6:
        b += k[2];
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 5:
        b += k8[4]; /* fall through */
      case 4:
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 3:
        a += ((uint32_t)k8[2]) << 16; /* fall through */
      case 2:
        a += k[0];
        break;
      case 1:
        a += k8[0];
        break;
      case 0:
        *idx1 = c;
        *idx2 = b;
        return; /* zero length strings require no mixing */
    }

  } else { /* need to read the key one byte at a time */
    const uint8_t *k = (const uint8_t *)buf;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      a += ((uint32_t)k[1]) << 8;
      a += ((uint32_t)k[2]) << 16;
      a += ((uint32_t)k[3]) << 24;
      b += k[4];
      b += ((uint32_t)k[5]) << 8;
      b += ((uint32_t)k[6]) << 16;
      b += ((uint32_t)k[7]) << 24;
      c += k[8];
      c += ((uint32_t)k[9]) << 8;
      c += ((uint32_t)k[10]) << 16;
      c += ((uint32_t)k[11]) << 24;
      mix(a, b, c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length) /* all the case statements fall through */
    {
      case 12:
        c += ((uint32_t)k[11]) << 24;
      case 11:
        c += ((uint32_t)k[10]) << 16;
      case 10:
        c += ((uint32_t)k[9]) << 8;
      case 9:
        c += k[8];
      case 8:
        b += ((uint32_t)k[7]) << 24;
      case 7:
        b += ((uint32_t)k[6]) << 16;
      case 6:
        b += ((uint32_t)k[5]) << 8;
      case 5:
        b += k[4];
      case 4:
        a += ((uint32_t)k[3]) << 24;
      case 3:
        a += ((uint32_t)k[2]) << 16;
      case 2:
        a += ((uint32_t)k[1]) << 8;
      case 1:
        a += k[0];
        break;
      case 0:
        *idx1 = c;
        *idx2 = b;
        return; /* zero length strings require no mixing */
    }
  }

  final(a, b, c);
  *idx1 = c;
  *idx2 = b;
}

void HashUtil::BobHash(const std::string &s, uint32_t *idx1, uint32_t *idx2) {
  return BobHash(s.data(), s.length(), idx1, idx2);
}

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby
// Note - This code makes a few assumptions about how your machine behaves -
// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4
// And it has a few limitations -
// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.
// All code is released to the public domain. For business purposes,
// Murmurhash is under the MIT license.

uint32_t HashUtil::MurmurHash(const void *buf, size_t len, uint32_t seed) {
  // 'm' and 'r' are mixing constants generated offline.
  // They're not really 'magic', they just happen to work well.

  const unsigned int m = 0x5bd1e995;
  const int r = 24;

  // Initialize the hash to a 'random' value
  uint32_t h = seed ^ len;

  // Mix 4 bytes at a time into the hash
  const unsigned char *data = (const unsigned char *)buf;

  while (len >= 4) {
    unsigned int k = *(unsigned int *)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  // Handle the last few bytes of the input array
  switch (len) {
    case 3:
      h ^= data[2] << 16;
    case 2:
      h ^= data[1] << 8;
    case 1:
      h ^= data[0];
      h *= m;
  };

  // Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
}

uint32_t HashUtil::MurmurHash(const std::string &s, uint32_t seed) {
  return MurmurHash(s.data(), s.length(), seed);
}

// SuperFastHash aka Hsieh Hash, License: GPL 2.0
uint32_t HashUtil::SuperFastHash(const void *buf, size_t len) {
  const char *data = (const char *)buf;
  uint32_t hash = len, tmp;
  int rem;

  if (len == 0 || data == NULL) return 0;

  rem = len & 3;
  len >>= 2;

  /* Main loop */
  for (; len > 0; len--) {
    hash += get16bits(data);
    tmp = (get16bits(data + 2) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    data += 2 * sizeof(uint16_t);
    hash += hash >> 11;
  }

  /* Handle end cases */
  switch (rem) {
    case 3:
      hash += get16bits(data);
      hash ^= hash << 16;
      hash ^= data[sizeof(uint16_t)] << 18;
      hash += hash >> 11;
      break;
    case 2:
      hash += get16bits(data);
      hash ^= hash << 11;
      hash += hash >> 17;
      break;
    case 1:
      hash += *data;
      hash ^= hash << 10;
      hash += hash >> 1;
  }

  /* Force "avalanching" of final 127 bits */
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;

  return hash;
}

uint32_t HashUtil::SuperFastHash(const std::string &s) {
  return SuperFastHash(s.data(), s.length());
}

uint32_t HashUtil::NullHash(const void *buf, size_t length,
                            uint32_t shiftbytes) {
  // Ensure that enough bits exist in buffer
  if (length - shiftbytes < sizeof(uint32_t)) {
    return 0;
  }
  char *data = (char *)buf;
  return ((data[(length - shiftbytes - 4)] << 24) +
          (data[(length - shiftbytes - 3)] << 16) +
          (data[(length - shiftbytes - 2)] << 8) +
          (data[(length - shiftbytes - 1)]));
}

/*
 * Compatibility layer for OpenSSL < 1.1.0.
 * Implemented as proposed by https://wiki.openssl.org/index.php/OpenSSL_1.1.0_Changes
 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include <string.h>
static void *OPENSSL_zalloc(size_t num)
{
  void *ret = OPENSSL_malloc(num);

  if (ret != NULL)
      memset(ret, 0, num);
  return ret;
}

EVP_MD_CTX *EVP_MD_CTX_new(void)
{
   return (EVP_MD_CTX *)OPENSSL_zalloc(sizeof(EVP_MD_CTX));
}

void EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
   EVP_MD_CTX_cleanup(ctx);
   OPENSSL_free(ctx);
}
#endif

std::string HashUtil::MD5Hash(const char *inbuf, size_t in_length) {
  EVP_MD_CTX *mdctx;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  mdctx = EVP_MD_CTX_new();
  EVP_DigestInit(mdctx, EVP_md5());
  EVP_DigestUpdate(mdctx, (const void *)inbuf, in_length);
  EVP_DigestFinal_ex(mdctx, md_value, &md_len);
  EVP_MD_CTX_free(mdctx);

  return std::string((char *)md_value, (size_t)md_len);
}

std::string HashUtil::SHA1Hash(const char *inbuf, size_t in_length) {
  EVP_MD_CTX *mdctx;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  mdctx = EVP_MD_CTX_new();
  EVP_DigestInit(mdctx, EVP_sha1());
  EVP_DigestUpdate(mdctx, (const void *)inbuf, in_length);
  EVP_DigestFinal_ex(mdctx, md_value, &md_len);
  EVP_MD_CTX_free(mdctx);

  return std::string((char *)md_value, (size_t)md_len);
}
}  // namespace cuckoofilter
