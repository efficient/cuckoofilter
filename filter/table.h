/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _TABLE_H_
#define _TABLE_H_
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace hashfilter {

    // interface of the hashtable used by hashfilter
    class Table {

    public:
        // // total number of buckets in this table
        // size_t num_buckets;

        // // associativity
        // size_t tag_per_bucket;

        // // max  number of tags stored in this table
        // size_t num_tags;

        // // number of bits each tag takes
        // size_t bit_per_tag;

        // // mask code for a valid tag
        // uint32_t TAGMASK;


        Table() {}

        virtual ~Table() {};


        // check if a given tag is in bucket i,
        // if yes, return true;
        // if no,  return false.
        virtual  bool  FindTagInBucket(const size_t i,  const uint32_t tag) const = 0; 

        // insert the new tag to bucket i if there is still space, 
        // return true (false) if it succeeds (or unsucceeds); 
        // for unsuccessful lookup, kickout is true,  replace a random existing tag and store it in  a old_tag,
        virtual  bool  InsertTagToBucket(const size_t i,  const uint32_t tag, 
                                         const bool kickout, uint32_t& oldtag) = 0; 

        virtual void CleanupTags() = 0;

        // number of bytes of the hashtable
        virtual size_t SizeInByte() const = 0;

        virtual size_t SizeInTag() const = 0;

        // a string of summary of the hashtable
        virtual std::string Info() const = 0;
    };
}

#endif // #ifndef _TABLE_H_
