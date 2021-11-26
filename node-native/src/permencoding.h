#ifndef CUCKOO_FILTER_PERM_ENCODING_H_
#define CUCKOO_FILTER_PERM_ENCODING_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "debug.h"

namespace cuckoofilter {

class PermEncoding {
  /* unpack one 2-byte number to four 4-bit numbers */
  // inline void unpack(const uint16_t in, const uint8_t out[4]) const {
  //     (*(uint16_t *)out)      = in & 0x0f0f;
  //     (*(uint16_t *)(out +2)) = (in >> 4) & 0x0f0f;
  // }

  inline void unpack(uint16_t in, uint8_t out[4]) const {
    out[0] = (in & 0x000f);
    out[2] = ((in >> 4) & 0x000f);
    out[1] = ((in >> 8) & 0x000f);
    out[3] = ((in >> 12) & 0x000f);
  }

  /* pack four 4-bit numbers to one 2-byte number */
  inline uint16_t pack(const uint8_t in[4]) const {
    uint16_t in1 = *((uint16_t *)(in)) & 0x0f0f;
    uint16_t in2 = *((uint16_t *)(in + 2)) << 4;
    return in1 | in2;
  }

 public:
  PermEncoding() {
    uint8_t dst[4];
    uint16_t idx = 0;
    memset(dec_table, 0, sizeof(dec_table));
    memset(enc_table, 0, sizeof(enc_table));
    gen_tables(0, 0, dst, idx);
  }

  ~PermEncoding() {}

  static const size_t N_ENTS = 3876;

  uint16_t dec_table[N_ENTS];
  uint16_t enc_table[1 << 16];

  inline void decode(const uint16_t codeword, uint8_t lowbits[4]) const {
    unpack(dec_table[codeword], lowbits);
  }

  inline uint16_t encode(const uint8_t lowbits[4]) const {
    if (DEBUG_ENCODE & debug_level) {
      printf("Perm.encode\n");
      for (int i = 0; i < 4; i++) {
        printf("encode lowbits[%d]=%x\n", i, lowbits[i]);
      }
      printf("pack(lowbits) = %x\n", pack(lowbits));
      printf("enc_table[%x]=%x\n", pack(lowbits), enc_table[pack(lowbits)]);
    }

    return enc_table[pack(lowbits)];
  }

  void gen_tables(int base, int k, uint8_t dst[4], uint16_t &idx) {
    for (int i = base; i < 16; i++) {
      /* for fast comparison in binary_search in little-endian machine */
      dst[k] = i;
      if (k + 1 < 4) {
        gen_tables(i, k + 1, dst, idx);
      } else {
        dec_table[idx] = pack(dst);
        enc_table[pack(dst)] = idx;
        if (DEBUG_ENCODE & debug_level) {
          printf("enc_table[%04x]=%04x\t%x %x %x %x\n", pack(dst), idx, dst[0],
                 dst[1], dst[2], dst[3]);
        }
        idx++;
      }
    }
  }
};
}  // namespace cuckoofilter
#endif  // CUCKOO_FILTER_PERM_ENCODING_H_
