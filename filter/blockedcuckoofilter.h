/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _BLOCKED_CUCKOO_FILTER_H_
#define _BLOCKED_CUCKOO_FILTER_H_

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
    class BlockedCuckooFilter {

        TableType table_;
        size_t num_keys;
        
        static const size_t BLOCK_NBITS      = 3;
        static const size_t BLOCK_SIZE       = 1 << BLOCK_NBITS;
        static const size_t MAX_TOTAL_CUCKOO = BLOCK_SIZE;
        static const size_t MAX_LOCAL_CUCKOO = 500;

        inline size_t IndexHash(const char* data) const {
            return ((uint32_t*) data)[1] & table_.INDEXMASK;
        }

        inline uint32_t TagHash(const char* data) const {
            // taghash is never 0
            uint32_t r = ((uint32_t*) data)[0] & table_.TAGMASK;
            r += (r == 0);
            return r;
        }

        inline size_t LocalAltIndex(const size_t index, const uint32_t tag) const {
            // 0x5bd1e995 is the hash constant from MurmurHash2
            uint32_t offset = (tag * 0x5bd1e995) >>  (32 - BLOCK_SIZE);
            return (index ^ offset) & table_.INDEXMASK;
        }

        inline size_t RemoteAltIndex(const size_t index, const uint32_t tag) const {
            // 0x5bd1e995 is the hash constant from MurmurHash2
            return (index ^ (tag * 0x5bd1e995)) & table_.INDEXMASK;
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

        explicit BlockedCuckooFilter(): num_keys(0) {
            victim.used = false;
        }

        ~BlockedCuckooFilter() { }

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

    }; // declaration of class BlockedCuckooFilter


    template <typename TableType>
    Status BlockedCuckooFilter<TableType>::Add(const Value& key) {
        DPRINTF(DEBUG_CUCKOO, "\nBlockedCuckooFilter::Add(key)\n");
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
    Status BlockedCuckooFilter<TableType>::_Add(const size_t i, const uint32_t tag) {
        DPRINTF(DEBUG_CUCKOO, "\nBlockedCuckooFilter::_Add(i, tag)\n");
        size_t curindex = i;
        uint32_t curtag = tag;
        uint32_t oldtag;

        // testing this:
        // size_t j  = AltIndex(curindex, curtag);
        // size_t n1 = table_.NumTagsInBucket(i);
        // size_t n2 = table_.NumTagsInBucket(j);
        // if (n2 < n1) 
        //     curindex = j;

        uint32_t count = 0;

        while (count < MAX_TOTAL_CUCKOO) {
            
            uint32_t local_kick = 0;
            while (local_kick < MAX_LOCAL_CUCKOO) {
                bool kickout = (count > 0);
                oldtag = 0;
                DPRINTF(DEBUG_CUCKOO, "BlockedCuckooFilter::Add i=%zu, tag=%s\n", curindex, PrintUtil::bytes_to_hex((char*) &tag, 4).c_str());
                if (table_.InsertTagToBucket(curindex, curtag, kickout, oldtag)) {
                    DPRINTF(DEBUG_CUCKOO, "BlockedCuckooFilter::Add Ok\n");
                    num_keys ++;
                    return Ok;
                }

                if (kickout) {
                    DPRINTF(DEBUG_CUCKOO, "BlockedCuckooFilter::Add local Cuckoo\n");
                    curtag = oldtag;
                }
                curindex = LocalAltIndex(curindex, curtag);
                local_kick ++;
                count ++;
            }
            DPRINTF(DEBUG_CUCKOO, "BlockedCuckooFilter::Add remote Cuckoo\n");
            curindex = RemoteAltIndex(curindex, curtag);
        }

        victim.index = curindex;
        victim.tag = curtag;
        victim.used = true;
        return Ok;
    }


    template <typename TableType>
    Status BlockedCuckooFilter<TableType>::Contain(const Value& key) const {
        return _Contain(key.data());
    }

    template <typename TableType>
    Status BlockedCuckooFilter<TableType>::_Contain(const char* pkey) const {
        bool found = false;
        size_t i1, i2;

        uint32_t tag = TagHash(pkey);
        i1 = IndexHash(pkey);
        i2 = LocalAltIndex(i1, tag);


        DPRINTF(DEBUG_CUCKOO, "\nBlockedCuckooFilter::_Contain, tag=%x, i1=%lu, i2=%lu\n", tag, i1, i2);

        found  = victim.used && (tag == victim.tag) && (i1 == victim.index || i2 == victim.index);

        if (found | table_.FindTagInBuckets(i1, i2, tag)) {
            return Ok;
        }
        else {
            return NotFound;
        }
    }

    template <typename TableType>
    Status BlockedCuckooFilter<TableType>::Delete(const Value& key) {
        ;
        size_t i1, i2;
        uint32_t tag = TagHash(key);
        i1 = IndexHash(key);
        i2 = LocalAltIndex(i1, tag);
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
    string BlockedCuckooFilter<TableType>::Info() const {
        stringstream ss;
        ss << "BlockedCuckooFilter Status:\n";
        ss << "\t\tBlock size: " << BLOCK_SIZE << endl;
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

#endif // #ifndef _BLOCKED_CUCKOO_FILTER_H_
