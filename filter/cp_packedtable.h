/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _CP_PACKED_TABLE_H_
#define _CP_PACKED_TABLE_H_

#include <stdio.h>
#include <string.h>

#include "singletable.h"
#include "packedtable.h"
#include "printutil.h"
#include "debug.h"
namespace hashfilter {

    // Cache partitioned packed hashtable:
    //     a cache-resident packed table + a memory-resident table
    template <size_t bits_per_ctag, size_t bits_per_mtag, size_t num_buckets>
    class CPPackedTable{
        static const size_t bits_per_tag = bits_per_ctag + bits_per_mtag;

    public:
        static const uint32_t INDEXMASK = num_buckets - 1;
        static const uint32_t  TAGMASK = (1 << bits_per_tag) - 1;
        static const uint32_t CTAGMASK = (1 << bits_per_ctag) - 1;
        static const uint32_t MTAGMASK =  TAGMASK ^ CTAGMASK;

        PackedTable<bits_per_ctag,  num_buckets, true> cachetable_;
        SingleTable<bits_per_mtag, 4, num_buckets, false>  memtable_;


        CPPackedTable(){
            CleanupTags();
        }

        ~CPPackedTable() { }

        void CleanupTags() { cachetable_.CleanupTags(); memtable_.CleanupTags(); }

        size_t SizeInTags() const { return cachetable_.SizeInTags(); }

        size_t SizeInBytes() const { return cachetable_.SizeInBytes() + memtable_.SizeInBytes();}

        void inline comparator(uint32_t& a, uint32_t& b,
                               uint32_t& c, uint32_t& d) {
            if ((a & 0x0f) > (b & 0x0f)) {
                uint32_t tmp1 = a; a = b; b = tmp1;
                uint32_t tmp2 = c; c = d; d = tmp2;
            }
        }

        inline void SortTags(uint32_t *ctags, uint32_t *mtags) {
            comparator(ctags[0], ctags[2], mtags[0], mtags[2]);
            comparator(ctags[1], ctags[3], mtags[1], mtags[3]);
            comparator(ctags[0], ctags[1], mtags[0], mtags[1]);
            comparator(ctags[2], ctags[3], mtags[2], mtags[3]);
            comparator(ctags[1], ctags[2], mtags[1], mtags[2]);
        }

        inline void ReadBucket(const size_t i, 
                               uint32_t ctags[4], 
                               uint32_t mtags[4]) const {
            cachetable_.ReadBucket(i, ctags);
            mtags[0] = memtable_.ReadTag(i, 0);
            mtags[1] = memtable_.ReadTag(i, 1);
            mtags[2] = memtable_.ReadTag(i, 2);
            mtags[3] = memtable_.ReadTag(i, 3);
        }

        inline void WriteBucket(const size_t i, uint32_t ctags[4], uint32_t mtags[4]) {
            /* first sort the tags in increasing order */
            SortTags(ctags, mtags);
            cachetable_.WriteBucket(i, ctags, false);
            memtable_.WriteTag(i, 0, mtags[0]);
            memtable_.WriteTag(i, 1, mtags[1]);
            memtable_.WriteTag(i, 2, mtags[2]);
            memtable_.WriteTag(i, 3, mtags[3]);
        }

        bool FindTagInBuckets(const size_t i1, 
                              const size_t i2,
                              const uint32_t tag) const {

            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag  & MTAGMASK) >> bits_per_ctag;

            uint32_t ctags1[4];
            uint32_t ctags2[4];
            cachetable_.ReadBucket(i1, ctags1);
            cachetable_.ReadBucket(i2, ctags2);
            for (size_t j = 0; j < 4; j++ ){
                if ((ctags1[j] == ctag) && (memtable_.ReadTag(i1, j) == mtag))
                    return true;
                if ((ctags2[j] == ctag) && (memtable_.ReadTag(i2, j) == mtag))
                    return true;

            }
            return false;
        }// FindTagInBuckets

        bool FindTagInBucket(const size_t i, const uint32_t tag) const {

            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag  & MTAGMASK) >> bits_per_ctag;

            uint32_t ctags[4];
            cachetable_.ReadBucket(i, ctags);
            for (size_t j = 0; j < 4; j++ ){
                if ((ctags[j] == ctag) && (memtable_.ReadTag(i, j) == mtag))
                    return true;
            }
            return false;
        }// FindTagInBucket


        bool DeleteTagFromBucket(const size_t i, const uint32_t tag) {

            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag  & MTAGMASK) >> bits_per_ctag;

            uint32_t ctags[4];
            uint32_t mtags[4];
            cachetable_.ReadBucket(i, ctags);
            for (size_t j = 0; j < 4; j++ ){
                if ((ctags[j] == ctag) && (memtable_.ReadTag(i, j) == mtag)) {
                    ctags[j] = 0;
                    mtags[j] = 0;
                    WriteBucket(i, ctags, mtags);
                    return true;
                }
            }
            return false;
        }// DeleteTagFromBucket

        bool InsertTagToBucket(const size_t i,  const uint32_t tag, 
                               const bool kickout, uint32_t& oldtag)  {
            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag  & MTAGMASK) >> bits_per_ctag;

            uint32_t ctags[4];
            uint32_t mtags[4];
            ReadBucket(i, ctags, mtags);

            for (size_t j = 0; j < 4; j++ ){
                if (ctags[j] == 0 && mtags[j] == 0) {
                    //  this is a empty slot
                    ctags[j] = ctag;
                    mtags[j] = mtag;
                    WriteBucket(i, ctags, mtags);
                    return true;
                }
            }
            if (kickout) {
                size_t r = rand() % 4;
                oldtag = ctags[r] | (mtags[r] << bits_per_ctag);
                ctags[r] = ctag;
                mtags[r] = mtag;
                WriteBucket(i, ctags, mtags);
            }
            return false;
        }// InsertTagToBucket

        std::string Info() const {
            std::stringstream ss;
            ss << "CachePartitioned PackedHashTable \n";
            ss << "\t\tIn-Cache table is:\t";
            ss << cachetable_.Info();
            ss << "\t\tIn-Memory table is:\t";
            ss << memtable_.Info();
            return ss.str();
        }

    }; // class CPPackedTable
}

#endif // #ifndef _CP_PACKED_TABLE_H_
