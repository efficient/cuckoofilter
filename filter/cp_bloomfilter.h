/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _CPBLOOMFILTER_H_
#define _CPBLOOMFILTER_H_

#include "filter_common.h"

namespace hashfilter {

    /*
     * A naive implementation of cache partitioned bloomfilter
     * first two hash functions are from the bits of the pre-hashed keys
     * the other hash functions are calculated in the cheap ways as 
     * described in "Less Hashing, Same Performance: Building a Better Bloom Filter"
     * by Adam Kirsch and Michael Mitzenmacher
     */
    template <size_t num_cbits, 
              size_t num_mbits,
              size_t num_hashes>
    class CPBloomFilter {
        static const size_t num_cbytes = num_cbits >> 3;
        static const size_t num_mbytes = num_mbits >> 3;
        static const uint32_t cINDEXMASK = num_cbits - 1;
        static const uint32_t mINDEXMASK = num_mbits - 1;
        char cbits_[num_cbytes];
        char mbits_[num_mbytes];

        inline size_t Hash1(const char* data) const {
            return ((uint32_t*) data)[0] & cINDEXMASK;
        }

        inline size_t Hash2(const char* data) const {
            return ((uint32_t*) data)[1] & cINDEXMASK;
        }
    
        inline void _SetCBit(size_t i) {
            cbits_[i >> 3] |= op[i & 0x7];
        }

        inline void _SetMBit(size_t i) {
            char* p = mbits_ + (i >> 3);
            _mm_prefetch(p, _MM_HINT_NTA);
            (*p) |= op[i & 0x7];
        }

        inline unsigned char _ReadCBit(size_t i) {
            return (cbits_[i >> 3] & op[i & 0x7]);
        }

        inline unsigned char _ReadMBit(size_t i) {
            char* p = mbits_ + (i >> 3);
            _mm_prefetch(p, _MM_HINT_NTA);
            return ((*p) & op[i & 0x7]);
        }

    public:
        size_t num_keys;

        CPBloomFilter () {
            Reset();
        }

        ~CPBloomFilter() {}

        // number of current inserted keys;
        size_t Size() const {return num_keys;}

        size_t SizeInBytes() const { return num_cbytes + num_mbytes; }

        std::string Info() const  {
            std::stringstream ss;
            ss << "BloomFilter Status:" << endl;
            ss << "\t\tKeys stored: " << num_keys << endl;
            ss << "\t\tLoad Factor: " << LoadFactor() << endl;
            ss << "\t\tBloomfilter size:"<< (SizeInBytes() >> 10 )  << " KB\n";
            return ss.str();
        }

        void Reset() { 
            num_keys = 0;
            memset(cbits_, 0, num_cbytes); 
            memset(mbits_, 0, num_mbytes); 
        }

        Status Add(const Value& key) {
            size_t i1 = Hash1(key.data());
            size_t i2 = Hash2(key.data());
            _SetCBit(i1);
            _SetCBit(i2);
            for (size_t j = 2; j < num_hashes; j++) {
                size_t i = (i1 + j * i2) & mINDEXMASK;
                _SetMBit(i);
            }
            num_keys ++;
            return Ok;
        }

        Status Contain(const Value& key) {
            size_t i1 = Hash1(key.data());
            size_t i2 = Hash2(key.data());
            if (_ReadCBit(i1)  && _ReadCBit(i2)) {
                return Ok;
            }
            for (size_t j = 2; j < num_hashes; j++) {
                size_t i = (i1 + j * i2) & mINDEXMASK;
                if (!_ReadMBit(i)) return NotFound;
            }
            return NotFound;
        }

        double LoadFactor() const {
            size_t num_ones[] = {0, 1, 1, 2, 1, 2, 2, 3,
                                 1, 2, 2, 3, 2, 3, 3, 4};
            double  a = 0;
            for (size_t i = 0; i < num_cbytes; i++) {
                a += num_ones[cbits_[i] & 0xf];
                a += num_ones[(cbits_[i] >> 4) & 0xf];
            }
            for (size_t i = 0; i < num_mbytes; i++) {
                a += num_ones[mbits_[i] & 0xf];
                a += num_ones[(mbits_[i] >> 4) & 0xf];
            }
            return a / (num_cbits + num_mbits); 
        }

    }; // bloomfilter
}

#endif // _CPBLOOMFILTER_H_
