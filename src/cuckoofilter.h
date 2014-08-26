/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _CUCKOO_FILTER_H_
#define _CUCKOO_FILTER_H_

#include "debug.h"
#include "hashutil.h"
#include "packedtable.h"
#include "printutil.h"
#include "singletable.h"

#include <cassert>

using namespace std;

namespace cuckoofilter {
    // status returned by a cuckoo filter operation
    enum Status {
        Ok = 0,
        NotFound = 1,
        NotEnoughSpace = 2,
        NotSupported = 3,
    };

    // A cuckoo filter class exposes a Bloomier filter interface,
    // providing methods of Add, Delete, Contain. It takes three
    // template parameters:
    //   ItemType:  the type of item you want to insert
    //   bits_per_item: how many bits each item is hashed into
    //   TableType: the storage of table, SingleTable by default, and
    // PackedTable to enable semi-sorting 
    template <typename ItemType,
              size_t bits_per_item,
              template<size_t> class TableType = SingleTable>
    class CuckooFilter {
        TableType<bits_per_item> *table_;
        size_t  num_keys;

        static const size_t kMaxCuckooCount = 500;

        inline size_t IndexHash(uint32_t hv) const {
            return hv % table_->num_buckets;
        }

        inline uint32_t TagHash(uint32_t hv) const {
            uint32_t tag;
            tag = hv & ((1ULL << bits_per_item) - 1);
            tag += (tag == 0);
            return tag;
        }

        inline void IndexTagHash(const ItemType &key,
                                 size_t &index,
                                 uint32_t &tag) const {

            string hashed_key = HashUtil::SHA1Hash((const char*) &key,
                                                   sizeof(key));
            uint64_t hv = *((uint64_t*) hashed_key.c_str());

            index = IndexHash((uint32_t) (hv >> 32));
            tag   = TagHash((uint32_t) (hv & 0xFFFFFFFF));
        }

        inline size_t AltIndex(const size_t index, const uint32_t tag) const {
            // NOTE(binfan): originally we use:
            // index ^ HashUtil::BobHash((const void*) (&tag), 4)) & table_->INDEXMASK;
            // now doing a quick-n-dirty way:
            // 0x5bd1e995 is the hash constant from MurmurHash2
            return IndexHash((uint32_t) (index ^ (tag * 0x5bd1e995)));
        }

        struct {
            size_t index;
            uint32_t tag;
            bool used;
        } victim;

        Status AddImpl(const size_t i, const uint32_t tag);

        // load factor is the fraction of occupancy
        double LoadFactor() const {
            return 1.0 * Size()  / table_->SizeInTags();
        }

        double BitsPerKey() const {
            return 8.0 * table_->SizeInBytes() / Size();
        }

    public:
        explicit CuckooFilter(size_t num_keys): num_keys(0) {
            size_t assoc = 4;
            size_t num_buckets = upperpower2(num_keys / assoc);
            double frac = (double) num_keys / num_buckets / assoc;
            if (frac > 0.96) {
                num_buckets <<= 1;
            }
            victim.used = false;
            table_  = new TableType<bits_per_item>(num_buckets);
        }

        ~CuckooFilter() {
            delete table_;
        }


        // Add a key to the filter.
        Status Add(const ItemType& key);

        // Report if the key is inserted, with false positive rate.
        Status Contain(const ItemType& key) const;

        // Delete a key from the hash table
        Status Delete(const ItemType& key);

        /* methods for providing stats  */
        // summary infomation
        string Info() const;

        // number of current inserted keys;
        size_t Size() const {return num_keys;}

        // size of the filter in bytes.
        size_t SizeInBytes() const {return table_->SizeInBytes();}
    }; // declaration of class CuckooFilter


    template <typename ItemType,
              size_t bits_per_item,
              template<size_t> class TableType>
    Status
    CuckooFilter<ItemType, bits_per_item, TableType>::Add(const ItemType& key) {
        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::Add(key)\n");
        if (victim.used) {
            DPRINTF(DEBUG_CUCKOO, "not enough space\n");
            return NotEnoughSpace;
        }
        size_t i;
        uint32_t tag;
        IndexTagHash(key, i, tag);

        return AddImpl(i, tag);
    }

    template <typename ItemType,
              size_t bits_per_item,
              template<size_t> class TableType>
    Status
    CuckooFilter<ItemType, bits_per_item, TableType>::AddImpl(
        const size_t i, const uint32_t tag) {
        DPRINTF(DEBUG_CUCKOO, "\nCuckooFilter::AddImpl(i, tag)\n");
        size_t curindex = i;
        uint32_t curtag = tag;
        uint32_t oldtag;

        for (uint32_t count = 0; count < kMaxCuckooCount; count ++) {
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

    template <typename ItemType,
              size_t bits_per_item,
              template<size_t> class TableType>
    Status
    CuckooFilter<ItemType, bits_per_item, TableType>::Contain(const ItemType& key) const {
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

    template <typename ItemType,
              size_t bits_per_item,
              template<size_t> class TableType>
    Status
    CuckooFilter<ItemType, bits_per_item, TableType>::Delete(const ItemType& key) {
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
        else if (victim.used && tag == victim.tag &&
                 (i1 == victim.index || i2 == victim.index)) {
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
            AddImpl(i, tag);
        }
        return Ok;
    }

    template <typename ItemType,
              size_t bits_per_item,
              template<size_t> class TableType>
    string CuckooFilter<ItemType, bits_per_item, TableType>::Info() const {
        stringstream ss;
        ss << "CuckooFilter Status:\n";
#ifdef QUICK_N_DIRTY_HASHING
        ss << "\t\tQuick hashing used\n";
#else
        ss << "\t\tBob hashing used\n";
#endif
        ss << "\t\t" << table_->Info() << "\n";
        ss << "\t\tKeys stored: " << Size() << "\n";
        ss << "\t\tLoad facotr: " << LoadFactor() << "\n";
        ss << "\t\tHashtable size: " << (table_->SizeInBytes() >> 10)
           << " KB\n";
        if (Size() > 0) {
            ss << "\t\tbit/key:   " << BitsPerKey() << "\n";
        }
        else {
            ss << "\t\tbit/key:   N/A\n";
        }
        return ss.str();
    }
}  // namespace cuckoofilter

#endif // #ifndef _CUCKOO_FILTER_H_
