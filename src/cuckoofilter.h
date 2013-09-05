/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _CUCKOO_FILTER_H_
#define _CUCKOO_FILTER_H_

#include <cassert>

#include "hashutil.h"
#include "printutil.h"
#include "debug.h"


#include "singletable.h"

using namespace std;

namespace cuckoofilter {

    enum Status {
        Ok = 0,
        NotFound = 1,
        NotEnoughSpace = 2,
        NotSupported = 3,
    }; // status returned by cuckoo filter

    class HashUtil;

    // The logic to do partial-key cuckoo hashing.
    // to cope with different hashtables, e.g. 
    // cache partitioned, or permutation-encoded,
    // subclass Table and pass it to the constructor.
    template <typename KeyType, 
              size_t bits_per_key, 
              template<size_t> class TableType = SingleTable>
    class CuckooFilter {

        TableType<bits_per_key> *table_;
        size_t      num_keys;

        static const size_t MAX_CUCKOO_COUNT = 500;

        inline void IndexTagHash(const KeyType &key, size_t &index, uint32_t &tag) const {

            string hashed_key = HashUtil::SHA1Hash((const char*) &key, sizeof(key));
            uint64_t hv = *((uint64_t*) hashed_key.c_str());

            index = table_->IndexHash((uint32_t) (hv >> 32));
            tag   = table_->TagHash((uint32_t) (hv & 0xFFFFFFFF));
        }

        inline size_t AltIndex(const size_t index, const uint32_t tag) const {
            //
            // originally we use:
            // index ^ HashUtil::BobHash((const void*) (&tag), 4)) & table_->INDEXMASK;
            // now doing a quick-n-dirty way:
            // 0x5bd1e995 is the hash constant from MurmurHash2
            //
            return table_->IndexHash((uint32_t) (index ^ (tag * 0x5bd1e995)));
        }

        struct {
            size_t index;
            uint32_t tag;
            bool used;
        } victim;

        Status _Add(const size_t i, const uint32_t tag);

        // load factor is the fraction of occupancy
        double _LoadFactor() const { return 1.0 * Size()  / table_->SizeInTags(); }

        double _BitsPerKey() const { return 8.0 * table_->SizeInBytes() / Size(); }

    public:

        explicit CuckooFilter(size_t num_keys): num_keys(0) {
            size_t assoc       = 4;
            size_t num_buckets = upperpower2(num_keys / assoc);
            double frac        = (double) num_keys / num_buckets / assoc;
            if (frac > 0.96) {
                num_buckets <<= 1;
            }
            victim.used = false;
            table_      = new TableType<bits_per_key>(num_buckets);
        }

        ~CuckooFilter() {
            delete table_;
        }

        /*
         * A Bloomier filter interface:
         *    Add, Contain, Delete
         */

        // Add a key to the filter.
        Status Add(const KeyType& key);

        // Report if the key is inserted, with false positive rate.
        Status Contain(const KeyType& key) const;

        // Delete a key from the hash table
        Status Delete(const KeyType& key);


        /* methods for providing stats  */
        // summary infomation
        string Info() const;

        // number of current inserted keys;
        size_t Size() const {return num_keys;}

        // size of the filter in bytes.
        size_t SizeInBytes() const {return table_->SizeInBytes();}

    }; // declaration of class CuckooFilter


    //template <typename KeyType>
    template <typename KeyType, 
              size_t bits_per_key, 
              template<size_t> class TableType>
    Status CuckooFilter<KeyType, bits_per_key, TableType>::Add(const KeyType& key) {
        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::Add(key)\n");
        if (victim.used) {
            DPRINTF(DEBUG_CUCKOO, "not enough space\n");
            return NotEnoughSpace;
        }
        size_t i;
        uint32_t tag;
        IndexTagHash(key, i, tag);

        return _Add(i, tag);
    }

    //template <typename KeyType>
    template <typename KeyType, 
              size_t bits_per_key, 
              template<size_t> class TableType>
    Status CuckooFilter<KeyType, bits_per_key, TableType>::_Add(const size_t i, const uint32_t tag) {
        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::_Add(i, tag)\n");
        size_t curindex = i;
        uint32_t curtag = tag;
        uint32_t oldtag;

        for (uint32_t count = 0; count < MAX_CUCKOO_COUNT; count ++) {
            bool kickout = (count > 0);
            oldtag = 0;
            DPRINTF(DEBUG_CUCKOO, "CuckooFilter::Add i=%zu, tag=%s\n", curindex, PrintUtil::bytes_to_hex((char*) &tag, 4).c_str());
            if (table_->InsertTagToBucket(curindex, curtag, kickout, oldtag)) {
                num_keys ++;
                DPRINTF(DEBUG_CUCKOO, "CuckooFilter::Add Ok\n");
                return Ok;
            }

            if (kickout) {
                DPRINTF(DEBUG_CUCKOO, "CuckooFilter::Add Cuckooing\n");
                curtag = oldtag;
            }
            curindex = AltIndex(curindex, curtag);
        }

        victim.index = curindex;
        victim.tag = curtag;
        victim.used = true;
        return Ok;
    }


    //template <typename KeyType>
    template <typename KeyType, 
              size_t bits_per_key, 
              template<size_t> class TableType>
    Status CuckooFilter<KeyType, bits_per_key, TableType>::Contain(const KeyType& key) const {
        bool found = false;
        size_t i1, i2;
        uint32_t tag;

        IndexTagHash(key, i1, tag);
        i2 = AltIndex(i1, tag);

        assert(i1 == AltIndex(i2, tag));

        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::_Contain, tag=%x, i1=%lu, i2=%lu\n", tag, i1, i2);

        found = victim.used && (tag == victim.tag) && (i1 == victim.index || i2 == victim.index);

        if (found || table_->FindTagInBuckets(i1, i2, tag)) {
            return Ok;
        }
        else {
            return NotFound;
        }
    }

    //template <typename KeyType>
    template <typename KeyType, 
              size_t bits_per_key, 
              template<size_t> class TableType>
    Status CuckooFilter<KeyType, bits_per_key, TableType>::Delete(const KeyType& key) {
        size_t i1, i2;
        uint32_t tag;

        IndexTagHash(key, i1, tag);
        i2 = AltIndex(i1, tag);

        if (table_->DeleteTagFromBucket(i1, tag))  {
            num_keys--;
            goto TryEliminateVictim;
        }
        else if (table_->DeleteTagFromBucket(i2, tag))  {
            num_keys--;
            goto TryEliminateVictim;
        }
        else if (victim.used && tag == victim.tag && (i1 == victim.index || i2 == victim.index)) {
            //num_keys --;
            victim.used = false;
            return Ok;
        }
        else {
            return NotFound;
        }
    TryEliminateVictim:
        if (victim.used) {
            victim.used = false;
            size_t i = victim.index;
            uint32_t tag = victim.tag;
            _Add(i, tag);
        }
        return Ok;
    }

    //template <typename KeyType>
    template <typename KeyType, 
              size_t bits_per_key, 
              template<size_t> class TableType>
    string CuckooFilter<KeyType, bits_per_key, TableType>::Info() const {
        stringstream ss;
        ss << "CuckooFilter Status:\n";
#ifdef QUICK_N_DIRTY_HASHING
        ss << "\t\tQuick hashing used\n";
#else
        ss << "\t\tBob hashing used\n";
#endif
        ss << "\t\t" << table_->Info() << "\n";
        ss << "\t\tKeys stored: " << Size() << "\n";
        ss << "\t\tLoad facotr: " << _LoadFactor() << "\n";
        ss << "\t\tHashtable size: " << (table_->SizeInBytes() >> 10) << " KB\n";
        if (Size() > 0) {
            ss << "\t\tbit/key:   " << _BitsPerKey() << "\n";
        }
        else {
            ss << "\t\tbit/key:   N/A\n";
        }
        return ss.str();
    }

} // namespace cuckoofilter

#endif // #ifndef _CUCKOO_FILTER_H_
