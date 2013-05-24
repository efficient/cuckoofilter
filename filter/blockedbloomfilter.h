/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _BLOCKEDBLOOMFILTER_H_
#define _BLOCKEDBLOOMFILTER_H_

#include "filter_common.h"

namespace hashfilter {

    
    // a naive implementation of bloomfilter
    template <size_t num_bits, size_t num_func>
    class BlockedBloomFilter {

        static const size_t bytes_per_block = 64;
        static const size_t bits_per_block  = bytes_per_block * 8;
        static const size_t num_blocks      = (num_bits + bits_per_block  - 1) / bits_per_block;
        static const size_t num_bytes       = num_bits >> 3;
        static const uint32_t block_mask    = bits_per_block - 1;
        char *bits_;

        inline size_t BlockIndex(const char* data) const {
            return ((uint32_t*) data)[0] % num_blocks;
        }

        inline size_t Hash1(const char* data) const {
            return ((uint32_t*) data)[1];
        }

        inline size_t Hash2(const char* data) const {
            return ((uint32_t*) data)[1] >> 16;
        }


        inline size_t CheapHash(const size_t i1, const size_t i2,  const size_t k) const {
            assert(k <= 20); 
            return  (i1 + rot[k] * i2) & block_mask;
        }

        inline void _SetBit(char* filter, size_t i) {
            filter[i >> 3] |= op[i & 0x7];
        }

        inline unsigned char _ReadBit(char* filter, size_t i) const {
            return filter[i >> 3] & op[i & 0x7];
        }

    public:
        size_t num_keys;
        std::mt19937 rng;

        BlockedBloomFilter () {
            bits_ = new char [num_bytes];
            Reset();
        }

        ~BlockedBloomFilter() {
            delete [] bits_;
        }

        // number of current inserted keys;
        size_t Size() const {return num_keys;}

        size_t SizeInBytes() const { return num_bytes; }

        std::string Info() const  {
            std::stringstream ss;
            ss << "BloomFilter Status:" << endl;
            ss << "\t\tHash Functions: " << num_func << endl;
            ss << "\t\tKeys stored: " << num_keys << endl;
            ss << "\t\tLoad Factor: " << LoadFactor() << endl;
            ss << "\t\tBloomfilter size:"<< (SizeInBytes() >> 10 )  << " KB\n";
            return ss.str();
        }

        void Reset() { 
            num_keys = 0;
            memset(bits_, 0, num_bytes); 
        }

        Status Add(const Value& key) {
            char* filter = bits_ + bytes_per_block * BlockIndex(key.data());

            size_t i1 = Hash1(key.data());
            size_t i2 = Hash2(key.data());
            _SetBit(filter, i1 & block_mask);
            _SetBit(filter, i2 & block_mask);
            for (size_t k = 2; k < num_func; k ++) {
                _SetBit(filter, CheapHash(i1, i2, k) & block_mask);
            }
            num_keys++;
            return Ok;
        }

        Status Contain(const Value& key) {
            char* filter = bits_ + bytes_per_block * BlockIndex(key.data());

            size_t i1 = Hash1(key.data());
            if (!_ReadBit(filter, i1 & block_mask)) {
                return NotFound;
            }

            size_t i2 = Hash2(key.data());
            if (!_ReadBit(filter, i2 & block_mask)) {
                return NotFound;
            }

            for (size_t k = 2; k < num_func; k ++) {
                if (!_ReadBit(filter, CheapHash(i1, i2, k) & block_mask)) {
                    return NotFound;
                }
            }
            return Ok;
        }

        double LoadFactor() const {
            size_t num_ones[] = {0, 1, 1, 2, 1, 2, 2, 3,
                                 1, 2, 2, 3, 2, 3, 3, 4};
            double  a = 0;
            for (size_t i = 0; i < num_bytes; i++) {
                a += num_ones[bits_[i] & 0xf];
                a += num_ones[(bits_[i] >> 4) & 0xf];
            }
            return a / num_bits; 
        }
    }; // bloomfilter
}

#endif // _BLOCKEDBLOOMFILTER_H_
