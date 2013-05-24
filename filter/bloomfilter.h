/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _BLOOMFILTER_H_
#define _BLOOMFILTER_H_

#include "filter_common.h"

namespace hashfilter {

    //
    // implementation of standard bloomfilter
    //
    template <size_t num_bits, size_t num_func>
    class BloomFilter {
        static const size_t num_bytes = (num_bits + 7) >> 3;
        char *bits_;

        inline size_t Hash1(const char* data) const {
            return ((uint32_t*) data)[0] % num_bits;
        }

        inline size_t Hash2(const char* data) const {
            return ((uint32_t*) data)[1] % num_bits;
        }


        inline size_t CheapHash(const size_t i1, const size_t i2,  const size_t k) const {
            assert(k <= 20); 
            return  (i1 + rot[k] * i2) % num_bits;
        }

        inline void _SetBit(size_t i) {
            bits_[i >> 3] |= op[i & 0x7];
        }

        inline unsigned char _ReadBit(size_t i) const {
            return (bits_[i >> 3] & op[i & 0x7]);
        }

    public:
        size_t num_keys;
        std::mt19937 rng;

        BloomFilter () {
            bits_ = new char [num_bytes];
            Reset();
        }

        ~BloomFilter() {
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
            size_t i1 = Hash1(key.data());
            size_t i2 = Hash2(key.data());
            _SetBit(i1);
            _SetBit(i2);
            for (size_t k = 2; k < num_func; k++) {
                _SetBit(CheapHash(i1, i2, k));
            }
            num_keys++;
            return Ok;
        }

        Status Contain(const Value& key) {
            size_t i1 = Hash1(key.data());
            if (!_ReadBit(i1)) {
                return NotFound;
            }

            size_t i2 = Hash2(key.data());
            if (!_ReadBit(i2)) {
                return NotFound;
            }

            for (size_t k = 2; k < num_func; k ++) {
                if (!_ReadBit(CheapHash(i1, i2, k))) {
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

#endif // _BLOOMFILTER_H_
