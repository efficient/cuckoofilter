/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _CUCKOO_FILTER_H_
#define _CUCKOO_FILTER_H_

#include <vector>

#include "value.h"
#include "hashutil.h"


using namespace std;

namespace hashfilter {

    class Value;
    class HashUtil;

    // The logic to do partial-key cuckoo hashing.
    // to cope with different hashtables, e.g. cache partitioned, or permutation-encoded,
    // subclass Table and pass it to the constructor.
    template <typename TableType>
    class CuckooFilter {

        TableType table_;
        size_t num_keys;

        static const size_t MAX_CUCKOO_COUNT = 500;

        inline size_t IndexHash(const char* data) const {
            return ((uint32_t*) data)[1] & table_.INDEXMASK;
        }

        inline uint32_t TagHash(const char* data) const {
            // taghash is never 0
            uint32_t r = ((uint32_t*) data)[0] & table_.TAGMASK;
            r += (r == 0);
            return r;
        }

        inline size_t AltIndex(const size_t index, const uint32_t tag) const {
#ifdef QUICK_N_DIRTY_HASHING
            // 0x5bd1e995 is the hash constant from MurmurHash2

            //uint32_t t = tag & ((1 << 6) -1);
            return (index ^ (tag * 0x5bd1e995)) & table_.INDEXMASK;
            //return (index ^ tag) & table_.INDEXMASK;
#else
            return (index ^ HashUtil::BobHash((const void*) (&tag), 4)) & table_.INDEXMASK;
#endif
        }

        struct {
            size_t index;
            uint32_t tag;
            bool used;
        } victim;

        inline Status _Add(const size_t i, const uint32_t tag);

        // load factor is the fraction of occupancy
        double _LoadFactor() const { return 1.0 * Size()  / table_.SizeInTags(); }

        double _BitsPerKey() const { return 8.0 * table_.SizeInBytes() / Size(); }

    public:

        explicit CuckooFilter(): num_keys(0) {
            victim.used = false;
        }

        ~CuckooFilter() { }

        /* A Bloomier filter interface:
         * Add, Contain, Delete
         */

        // Add a key to the filter.
        Status Add(const Value& key);

        Status _Contain(const char* key) const;

        // Report if the key is inserted, with false positive rate.
        Status Contain(const Value& key) const;

        // Delete a key from the hash table
        Status Delete(const Value& key);


        /* methods for providing stats  */
        // summary infomation
        string Info() const;

        // number of current inserted keys;
        size_t Size() const {return num_keys;}

        // size of the filter in bytes.
        size_t SizeInBytes() const {return table_.SizeInBytes();}

    }; // declaration of class CuckooFilter


    template <typename TableType>
    Status CuckooFilter<TableType>::Add(const Value& key) {
        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::Add(key)\n");
        if (victim.used) {
            DPRINTF(DEBUG_CUCKOO, "not enough space\n");
            return NotEnoughSpace;
        }
        const char *pkey = key.data();
        size_t i = IndexHash(pkey);
        uint32_t tag = TagHash(pkey);

        return _Add(i, tag);
    }

    template <typename TableType>
    Status CuckooFilter<TableType>::_Add(const size_t i, const uint32_t tag) {
        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::_Add(i, tag)\n");
        size_t curindex = i;
        uint32_t curtag = tag;
        uint32_t oldtag;

        // testing this:
        // size_t j  = AltIndex(curindex, curtag);
        // size_t n1 = table_.NumTagsInBucket(i);
        // size_t n2 = table_.NumTagsInBucket(j);
        // if (n2 < n1) 
        //     curindex = j;



        for (uint32_t count = 0; count < MAX_CUCKOO_COUNT; count ++) {
            bool kickout = (count > 0);
            oldtag = 0;
            DPRINTF(DEBUG_CUCKOO, "CuckooFilter::Add i=%zu, tag=%s\n", curindex, PrintUtil::bytes_to_hex((char*) &tag, 4).c_str());
            if (table_.InsertTagToBucket(curindex, curtag, kickout, oldtag)) {
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


    template <typename TableType>
    Status CuckooFilter<TableType>::Contain(const Value& key) const {
        return _Contain(key.data());
    }

    template <typename TableType>
    Status CuckooFilter<TableType>::_Contain(const char* pkey) const {
        bool found = false;
        size_t i1, i2;

        uint32_t tag = TagHash(pkey);
        i1 = IndexHash(pkey);
        i2 = AltIndex(i1, tag);


        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::_Contain, tag=%x, i1=%lu, i2=%lu\n", tag, i1, i2);

        found  = victim.used && (tag == victim.tag) && (i1 == victim.index || i2 == victim.index);

        if (found | table_.FindTagInBuckets(i1, i2, tag)) {
            return Ok;
        }
        else {
            return NotFound;
        }
    }

    template <typename TableType>
    Status CuckooFilter<TableType>::Delete(const Value& key) {
        ;
        size_t i1, i2;
        uint32_t tag = TagHash(key);
        i1 = IndexHash(key);
        i2 = AltIndex(i1, tag);
        if (table_.DeleteTagFromBucket(i1, tag))  {
            num_keys --;
            goto TryEliminateVictim;
        }
        else if (table_.DeleteTagFromBucket(i2, tag))  {
            num_keys --;
            goto TryEliminateVictim;
        }
        else if (victim.used && tag == victim.tag && (i1 == victim.index || i2 == victim.index)) {
            //num_keys --;
            victim.used = false;
            return Ok;
        }
        else
            return NotFound;
    TryEliminateVictim:
        if (victim.used) {
            victim.used = false;
            size_t i = victim.index;
            uint32_t tag = victim.tag;
            _Add(i, tag);
        }
        return Ok;
    }

    template <typename TableType>
    string CuckooFilter<TableType>::Info() const {
        stringstream ss;
        ss << "CuckooFilter Status:\n";
#ifdef QUICK_N_DIRTY_HASHING
        ss << "\t\tQuick hashing used\n";
#else
        ss << "\t\tBob hashing used\n";
#endif
        ss << "\t\t" << table_.Info() << "\n";
        ss << "\t\tKeys stored: " << Size() << "\n";
        ss << "\t\tLoad facotr: " << _LoadFactor() << "\n";
        ss << "\t\tHashtable size: " << (table_.SizeInBytes() >> 10) << " KB\n";
        if (Size() > 0)
            ss << "\t\tbit/key:   " << _BitsPerKey() << "\n";
        else
            ss << "\t\tbit/key:   N/A\n";

        return ss.str();
    }

} // namespace hashfilter

#endif // #ifndef _CUCKOO_FILTER_H_
