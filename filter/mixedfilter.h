/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _MIXED_FILTER_H_
#define _MIXED_FILTER_H_

#include <stdio.h>
#include <string.h>

#include "bloomfilter.h"
#include "cuckoohashfilter.h"
#include "printutil.h"
#include "debug.h"

namespace hashfilter {
    // bloomfilter hashtable:
    //     a cache-resident bloomfilter + a memory-resident table
    template <size_t num_bits, typename TableType>
    class MixedFilter {
    public:
        BloomFilter<num_bits> cachefilter_;
        CuckooHashFilter<TableType> memfilter_;

        MixedFilter() { }

        ~MixedFilter() { }

        // Add a key to the filter.
        Status Add(const Value& key) {
            cachefilter_.Add(key);
            return  memfilter_.Add(key);
        }

        // Report if the key is inserted, with false positive rate.
        Status Contain(const Value& key) {
            if (cachefilter_.Contain(key) == Ok)
                return memfilter_.Contain(key);
            return NotFound;
        }

        // Delete a key from the hash table
        Status Delete(const Value& key) {
            // not supported yet
            return Ok;
        }

        string Info() const {
            stringstream ss;
            ss << "MixedFilter" << endl;
            ss << cachefilter_.Info() << endl;
            ss << memfilter_.Info() << endl;
            return ss.str();
        }

        // number of current inserted keys;
        size_t Size() const {return memfilter_.Size();}

        // size of the filter in bytes.
        size_t SizeInBytes() const {
            return cachefilter_.SizeInBytes() + memfilter_.SizeInBytes();
        }

    }; // mixed filter
}

#endif // #ifndef _MIXED_FILTER_
