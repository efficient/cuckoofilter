/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _BITS_H_
#define _BITS_H_

// inspired from http://www-graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
#define haszero4(x) (((x) - 0x1111ULL) & (~(x)) & 0x8888ULL)
#define hasvalue4(x,n) (haszero4((x) ^ (0x1111ULL * (n))))

#define haszero8(x) (((x) - 0x01010101ULL) & (~(x)) & 0x80808080ULL)
#define hasvalue8(x,n) (haszero8((x) ^ (0x01010101ULL * (n))))

#define haszero12(x) (((x) - 0x001001001001ULL) & (~(x)) & 0x800800800800ULL)
#define hasvalue12(x,n) (haszero12((x) ^ (0x001001001001ULL * (n))))

#define haszero16(x) (((x) - 0x0001000100010001ULL) & (~(x)) & 0x8000800080008000ULL)
#define hasvalue16(x,n) (haszero16((x) ^ (0x0001000100010001ULL * (n))))

/* inline bool hasvalue12(const uint64_t x, const uint32_t n) { */
/*     //return haszero12((x) ^ (0x001001001001ULL * (n))); */
/*     uint64_t t1 = (uint64_t) (n) << 36; */
/*     uint64_t t2 = (uint64_t) (n) << 24; */
/*     uint64_t t3 = (uint64_t) (n) << 12; */
/*     return haszero12((x) ^ (t1 + t2 + t3 + n)); */

/* } */


#endif //_BITS_H
