/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _CP_TABLE_H_
#define _CP_TABLE_H_

#include <stdio.h>
#include <string.h>

#include "singletable.h"
#include "printutil.h"
#include "debug.h"
namespace hashfilter {
    // Cache partitioned hashtable:
    //     a cache-resident table + a memory-resident table
    template <size_t bits_per_ctag, size_t bits_per_mtag, size_t tags_per_bucket,  size_t num_buckets>
    class CachePartitionedTable {
        static const size_t bits_per_tag = bits_per_ctag + bits_per_mtag;

    public:
        static const uint32_t INDEXMASK = num_buckets - 1;
        static const uint32_t  TAGMASK = (1 << bits_per_tag) - 1;
        static const uint32_t CTAGMASK = (1 << bits_per_ctag) - 1;
        static const uint32_t MTAGMASK =  TAGMASK ^ CTAGMASK;

        SingleTable<bits_per_ctag, tags_per_bucket, num_buckets, true> cachetable_;
        SingleTable<bits_per_mtag, tags_per_bucket, num_buckets, false>  memtable_;


        CachePartitionedTable() {
            CleanupTags();
        }

        ~CachePartitionedTable() { }

        void CleanupTags() { cachetable_.CleanupTags(); memtable_.CleanupTags(); }

        size_t SizeInTags() const { return cachetable_.SizeInTags();}

        size_t SizeInBytes() const { return cachetable_.SizeInBytes() + memtable_.SizeInBytes();}

        std::string Info() const {
            std::stringstream ss;
            ss << "CachePartitioned HashTable \n";
            ss << "\t\tIn-Cache table is:\t";
            ss << cachetable_.Info();
            ss << "\t\tIn-Memory table is:\t";
            ss << memtable_.Info();
            return ss.str();
        }

        // read tag from pos(i,j)
        inline uint32_t ReadTag(const size_t i, const size_t j) const {
            uint32_t ctag = cachetable_.ReadTag(i, j);
            uint32_t mtag = memtable_.ReadTag(i, j);
            uint32_t tag = (ctag + (mtag << bits_per_ctag)) & TAGMASK;
            return tag;
        }

        // write tag to pos(i,j)
        inline void  WriteTag(const size_t i, const size_t j, const uint32_t tag) {
            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag & MTAGMASK) >> bits_per_ctag;
            cachetable_.WriteTag(i, j, ctag);
            memtable_.WriteTag(i, j, mtag);
        }

        inline bool  FindTagInBuckets(const size_t i1, 
                                      const size_t i2,
                                      const uint32_t tag) const {
            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag & MTAGMASK) >> bits_per_ctag;

            for (size_t j = 0 ; j < tags_per_bucket; j++ ){
                if (cachetable_.ReadTag(i1, j) == ctag) 
                    if (memtable_.ReadTag(i1, j) == mtag) 
                        return true;
                if (cachetable_.ReadTag(i2, j) == ctag) 
                    if (memtable_.ReadTag(i2, j) == mtag) 
                        return true;
            }
            return false;
        }// FindTagInBuckets


        inline bool  FindTagInBucket(const size_t i, const uint32_t tag) const {
            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag & MTAGMASK) >> bits_per_ctag;

            for (size_t j = 0 ; j < tags_per_bucket; j++ ){
                if (cachetable_.ReadTag(i, j) == ctag) 
                    if (memtable_.ReadTag(i, j) == mtag) 
                        return true;
            }
            return false;
        }// FindTagInBucket

        inline  bool  DeleteTagFromBucket(const size_t i,  const uint32_t tag) {
            uint32_t ctag = tag & CTAGMASK;
            uint32_t mtag = (tag & MTAGMASK) >> bits_per_ctag;

            for (size_t j = 0; j < tags_per_bucket; j++ ){
                if (cachetable_.ReadTag(i, j) == ctag) 
                    if (memtable_.ReadTag(i, j) == mtag) {
                        cachetable_.WriteTag(i, j, 0);
                        memtable_.WriteTag(i, j, 0);
                        return true;
                    }
            }
            return false;
        }// DeleteTagFromBucket

        inline bool  InsertTagToBucket(const size_t i,  const uint32_t tag, 
                                         const bool kickout, uint32_t& oldtag)  {
            for (size_t j = 0; j < tags_per_bucket; j++ ){
                if (cachetable_.ReadTag(i, j) != 0)
                    continue;
                if (memtable_.ReadTag(i, j) == 0) {
                    WriteTag(i, j, tag);
                    return true;
                }
            }
            if (kickout) {
                size_t r = rand() % tags_per_bucket;
                oldtag = ReadTag(i, r);
                WriteTag(i, r, tag);
                return false;
            }
            return false;
        }// InsertTagToBucket

    }; // class CachePartitionedTable
}

#endif // #ifndef _CP_TABLE_H_
