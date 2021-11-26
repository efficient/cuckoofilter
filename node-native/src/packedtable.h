#ifndef CUCKOO_FILTER_PACKED_TABLE_H_
#define CUCKOO_FILTER_PACKED_TABLE_H_

#include <sstream>
#include <utility>

#include "debug.h"
#include "permencoding.h"
#include "printutil.h"

namespace cuckoofilter {

// Using Permutation encoding to save 1 bit per tag
template <size_t bits_per_tag>
class PackedTable {
  static const size_t kDirBitsPerTag = bits_per_tag - 4;
  static const size_t kBitsPerBucket = (3 + kDirBitsPerTag) * 4;
  static const size_t kBytesPerBucket = (kBitsPerBucket + 7) >> 3;
  static const uint32_t kDirBitsMask = ((1ULL << kDirBitsPerTag) - 1) << 4;

  // using a pointer adds one more indirection
  size_t len_;
  size_t num_buckets_;
  char *buckets_;
  PermEncoding perm_;

 public:
  explicit PackedTable(size_t num) : num_buckets_(num) {
    // NOTE(binfan): use 7 extra bytes to avoid overrun as we
    // always read a uint64
    len_ = kBytesPerBucket * num_buckets_ + 7;
    buckets_ = new char[len_];
    memset(buckets_, 0, len_); 
  }

  ~PackedTable() { 
    delete[] buckets_; 
  }

  size_t NumBuckets() const {
    return num_buckets_;
  }

  size_t SizeInTags() const { 
    return 4 * num_buckets_; 
  }

  size_t SizeInBytes() const { 
    return len_; 
  }

  std::string Info() const {
    std::stringstream ss;
    ss << "PackedHashtable with tag size: " << bits_per_tag << " bits";
    ss << "\t4 packed bits(3 bits after compression) and " << kDirBitsPerTag
       << " direct bits\n";
    ss << "\t\tAssociativity: 4\n";
    ss << "\t\tTotal # of rows: " << num_buckets_ << "\n";
    ss << "\t\ttotal # slots: " << SizeInTags() << "\n";
    return ss.str();
  }

  void PrintBucket(const size_t i) const {
    DPRINTF(DEBUG_TABLE, "PackedTable::PrintBucket %zu \n", i);
    const char *p = buckets_ + kBitsPerBucket * i / 8;
    std::cout << "\tbucketbits  ="
              << PrintUtil::bytes_to_hex((char *)p, kBytesPerBucket + 1)
              << std::endl;

    uint32_t tags[4];

    ReadBucket(i, tags);
    PrintTags(tags);
    DPRINTF(DEBUG_TABLE, "PackedTable::PrintBucket done \n");
  }

  void PrintTags(uint32_t tags[4]) const {
    DPRINTF(DEBUG_TABLE, "PackedTable::PrintTags \n");
    uint8_t lowbits[4];
    uint32_t dirbits[4];
    for (size_t j = 0; j < 4; j++) {
      lowbits[j] = tags[j] & 0x0f;
      dirbits[j] = (tags[j] & kDirBitsMask) >> 4;
    }
    uint16_t codeword = perm_.encode(lowbits);
    std::cout << "\tcodeword  ="
              << PrintUtil::bytes_to_hex((char *)&codeword, 2) << std::endl;
    for (size_t j = 0; j < 4; j++) {
      std::cout << "\ttag[" << j
                << "]: " << PrintUtil::bytes_to_hex((char *)&tags[j], 4);
      std::cout << " lowbits="
                << PrintUtil::bytes_to_hex((char *)&lowbits[j], 1)
                << " dirbits="
                << PrintUtil::bytes_to_hex((char *)&dirbits[j],
                                           kDirBitsPerTag / 8 + 1)
                << std::endl;
    }
    DPRINTF(DEBUG_TABLE, "PackedTable::PrintTags done\n");
  }

  inline void SortPair(uint32_t &a, uint32_t &b) {
    if ((a & 0x0f) > (b & 0x0f)) {
      std::swap(a, b);
    }
  }

  inline void SortTags(uint32_t *tags) {
    SortPair(tags[0], tags[2]);
    SortPair(tags[1], tags[3]);
    SortPair(tags[0], tags[1]);
    SortPair(tags[2], tags[3]);
    SortPair(tags[1], tags[2]);
  }

  /* read and decode the bucket i, pass the 4 decoded tags to the 2nd arg
   * bucket bits = 12 codeword bits + dir bits of tag1 + dir bits of tag2 ...
   */
  inline void ReadBucket(const size_t i, uint32_t tags[4]) const {
    DPRINTF(DEBUG_TABLE, "PackedTable::ReadBucket %zu \n", i);
    DPRINTF(DEBUG_TABLE, "kdirbitsMask=%x\n", kDirBitsMask);

    const char *p;  // =  buckets_ + ((kBitsPerBucket * i) >> 3);
    uint16_t codeword;
    uint8_t lowbits[4];

    if (bits_per_tag == 5) {
      // 1 dirbits per tag, 16 bits per bucket
      p = buckets_ + (i * 2);
      uint16_t bucketbits = *((uint16_t *)p);
      codeword = bucketbits & 0x0fff;
      tags[0] = ((bucketbits >> 8) & kDirBitsMask);
      tags[1] = ((bucketbits >> 9) & kDirBitsMask);
      tags[2] = ((bucketbits >> 10) & kDirBitsMask);
      tags[3] = ((bucketbits >> 11) & kDirBitsMask);
    } else if (bits_per_tag == 6) {
      // 2 dirbits per tag, 20 bits per bucket
      p = buckets_ + ((20 * i) >> 3);
      uint32_t bucketbits = *((uint32_t *)p);
      codeword = (*((uint16_t *)p) >> ((i & 1) << 2)) & 0x0fff;
      tags[0] = (bucketbits >> (8 + ((i & 1) << 2))) & kDirBitsMask;
      tags[1] = (bucketbits >> (10 + ((i & 1) << 2))) & kDirBitsMask;
      tags[2] = (bucketbits >> (12 + ((i & 1) << 2))) & kDirBitsMask;
      tags[3] = (bucketbits >> (14 + ((i & 1) << 2))) & kDirBitsMask;
    } else if (bits_per_tag == 7) {
      // 3 dirbits per tag, 24 bits per bucket
      p = buckets_ + (i << 1) + i;
      uint32_t bucketbits = *((uint32_t *)p);
      codeword = *((uint16_t *)p) & 0x0fff;
      tags[0] = (bucketbits >> 8) & kDirBitsMask;
      tags[1] = (bucketbits >> 11) & kDirBitsMask;
      tags[2] = (bucketbits >> 14) & kDirBitsMask;
      tags[3] = (bucketbits >> 17) & kDirBitsMask;
    } else if (bits_per_tag == 8) {
      // 4 dirbits per tag, 28 bits per bucket
      p = buckets_ + ((28 * i) >> 3);
      uint32_t bucketbits = *((uint32_t *)p);
      codeword = (*((uint16_t *)p) >> ((i & 1) << 2)) & 0x0fff;
      tags[0] = (bucketbits >> (8 + ((i & 1) << 2))) & kDirBitsMask;
      tags[1] = (bucketbits >> (12 + ((i & 1) << 2))) & kDirBitsMask;
      tags[2] = (bucketbits >> (16 + ((i & 1) << 2))) & kDirBitsMask;
      tags[3] = (bucketbits >> (20 + ((i & 1) << 2))) & kDirBitsMask;
    } else if (bits_per_tag == 9) {
      // 5 dirbits per tag, 32 bits per bucket
      p = buckets_ + (i * 4);
      uint32_t bucketbits = *((uint32_t *)p);
      codeword = *((uint16_t *)p) & 0x0fff;
      tags[0] = (bucketbits >> 8) & kDirBitsMask;
      tags[1] = (bucketbits >> 13) & kDirBitsMask;
      tags[2] = (bucketbits >> 18) & kDirBitsMask;
      tags[3] = (bucketbits >> 23) & kDirBitsMask;
    } else if (bits_per_tag == 13) {
      // 9 dirbits per tag,  48 bits per bucket
      p = buckets_ + (i * 6);
      uint64_t bucketbits = *((uint64_t *)p);
      codeword = *((uint16_t *)p) & 0x0fff;
      tags[0] = (bucketbits >> 8) & kDirBitsMask;
      tags[1] = (bucketbits >> 17) & kDirBitsMask;
      tags[2] = (bucketbits >> 26) & kDirBitsMask;
      tags[3] = (bucketbits >> 35) & kDirBitsMask;
    } else if (bits_per_tag == 17) {
      // 13 dirbits per tag, 64 bits per bucket
      p = buckets_ + (i << 3);
      uint64_t bucketbits = *((uint64_t *)p);
      codeword = *((uint16_t *)p) & 0x0fff;
      tags[0] = (bucketbits >> 8) & kDirBitsMask;
      tags[1] = (bucketbits >> 21) & kDirBitsMask;
      tags[2] = (bucketbits >> 34) & kDirBitsMask;
      tags[3] = (bucketbits >> 47) & kDirBitsMask;
    }

    /* codeword is the lowest 12 bits in the bucket */
    uint16_t v = perm_.dec_table[codeword];
    lowbits[0] = (v & 0x000f);
    lowbits[2] = ((v >> 4) & 0x000f);
    lowbits[1] = ((v >> 8) & 0x000f);
    lowbits[3] = ((v >> 12) & 0x000f);

    tags[0] |= lowbits[0];
    tags[1] |= lowbits[1];
    tags[2] |= lowbits[2];
    tags[3] |= lowbits[3];

    if (debug_level & DEBUG_TABLE) {
      PrintTags(tags);
    }
    DPRINTF(DEBUG_TABLE, "PackedTable::ReadBucket done \n");
  }

  /* Tag = 4 low bits + x high bits
   * L L L L H H H H ...
   */
  inline void WriteBucket(const size_t i, uint32_t tags[4], bool sort = true) {
    DPRINTF(DEBUG_TABLE, "PackedTable::WriteBucket %zu \n", i);
    /* first sort the tags in increasing order is arg sort = true*/
    if (sort) {
      DPRINTF(DEBUG_TABLE, "Sort tags\n");
      SortTags(tags);
    }
    if (debug_level & DEBUG_TABLE) {
      PrintTags(tags);
    }

    /* put in direct bits for each tag*/

    uint8_t lowbits[4];
    uint32_t highbits[4];

    lowbits[0] = tags[0] & 0x0f;
    lowbits[1] = tags[1] & 0x0f;
    lowbits[2] = tags[2] & 0x0f;
    lowbits[3] = tags[3] & 0x0f;

    highbits[0] = tags[0] & 0xfffffff0;
    highbits[1] = tags[1] & 0xfffffff0;
    highbits[2] = tags[2] & 0xfffffff0;
    highbits[3] = tags[3] & 0xfffffff0;

    // note that :  tags[j] = lowbits[j] | highbits[j]

    uint16_t codeword = perm_.encode(lowbits);
    DPRINTF(DEBUG_TABLE, "codeword=%s\n",
            PrintUtil::bytes_to_hex((char *)&codeword, 2).c_str());

    /* write out the bucketbits to its place*/
    const char *p = buckets_ + ((kBitsPerBucket * i) >> 3);
    DPRINTF(DEBUG_TABLE, "original bucketbits=%s\n",
            PrintUtil::bytes_to_hex((char *)p, 8).c_str());

    if (kBitsPerBucket == 16) {
      // 1 dirbits per tag
      *((uint16_t *)p) = codeword | (highbits[0] << 8) | (highbits[1] << 9) |
                         (highbits[2] << 10) | (highbits[3] << 11);
    } else if (kBitsPerBucket == 20) {
      // 2 dirbits per tag
      if ((i & 0x0001) == 0) {
        *((uint32_t *)p) &= 0xfff00000;
        *((uint32_t *)p) |= codeword | (highbits[0] << 8) |
                            (highbits[1] << 10) | (highbits[2] << 12) |
                            (highbits[3] << 14);
      } else {
        *((uint32_t *)p) &= 0xff00000f;
        *((uint32_t *)p) |= (codeword << 4) | (highbits[0] << 12) |
                            (highbits[1] << 14) | (highbits[2] << 16) |
                            (highbits[3] << 18);
      }
    } else if (kBitsPerBucket == 24) {
      // 3 dirbits per tag
      *((uint32_t *)p) &= 0xff000000;
      *((uint32_t *)p) |= codeword | (highbits[0] << 8) | (highbits[1] << 11) |
                          (highbits[2] << 14) | (highbits[3] << 17);
    } else if (kBitsPerBucket == 28) {
      // 4 dirbits per tag
      if ((i & 0x0001) == 0) {
        *((uint32_t *)p) &= 0xf0000000;
        *((uint32_t *)p) |= codeword | (highbits[0] << 8) |
                            (highbits[1] << 12) | (highbits[2] << 16) |
                            (highbits[3] << 20);
      } else {
        *((uint32_t *)p) &= 0x0000000f;
        *((uint32_t *)p) |= (codeword << 4) | (highbits[0] << 12) |
                            (highbits[1] << 16) | (highbits[2] << 20) |
                            (highbits[3] << 24);
      }
    } else if (kBitsPerBucket == 32) {
      // 5 dirbits per tag
      *((uint32_t *)p) = codeword | (highbits[0] << 8) | (highbits[1] << 13) |
                         (highbits[2] << 18) | (highbits[3] << 23);
      DPRINTF(DEBUG_TABLE, " new bucketbits=%s\n",
              PrintUtil::bytes_to_hex((char *)p, 4).c_str());
    } else if (kBitsPerBucket == 48) {
      // 9 dirbits per tag
      *((uint64_t *)p) &= 0xffff000000000000ULL;
      *((uint64_t *)p) |= codeword | ((uint64_t)highbits[0] << 8) |
                          ((uint64_t)highbits[1] << 17) |
                          ((uint64_t)highbits[2] << 26) |
                          ((uint64_t)highbits[3] << 35);
      DPRINTF(DEBUG_TABLE, " new bucketbits=%s\n",
              PrintUtil::bytes_to_hex((char *)p, 4).c_str());

    } else if (kBitsPerBucket == 64) {
      // 13 dirbits per tag
      *((uint64_t *)p) = codeword | ((uint64_t)highbits[0] << 8) |
                         ((uint64_t)highbits[1] << 21) |
                         ((uint64_t)highbits[2] << 34) |
                         ((uint64_t)highbits[3] << 47);
    }
    DPRINTF(DEBUG_TABLE, "PackedTable::WriteBucket done\n");
  }

  bool FindTagInBuckets(const size_t i1, const size_t i2,
                        const uint32_t tag) const {
    //            DPRINTF(DEBUG_TABLE, "PackedTable::FindTagInBucket %zu\n", i);
    uint32_t tags1[4];
    uint32_t tags2[4];

    // disable for now
    // _mm_prefetch( buckets_ + (i1 * kBitsPerBucket) / 8,  _MM_HINT_NTA);
    // _mm_prefetch( buckets_ + (i2 * kBitsPerBucket) / 8,  _MM_HINT_NTA);

    // ReadBucket(i1, tags1);
    // ReadBucket(i2, tags2);

    uint16_t v;
    uint64_t bucketbits1 = *((uint64_t *)(buckets_ + kBitsPerBucket * i1 / 8));
    uint64_t bucketbits2 = *((uint64_t *)(buckets_ + kBitsPerBucket * i2 / 8));

    tags1[0] = (bucketbits1 >> 8) & kDirBitsMask;
    tags1[1] = (bucketbits1 >> 17) & kDirBitsMask;
    tags1[2] = (bucketbits1 >> 26) & kDirBitsMask;
    tags1[3] = (bucketbits1 >> 35) & kDirBitsMask;
    v = perm_.dec_table[(bucketbits1)&0x0fff];
    // the order 0 2 1 3 is not a bug
    tags1[0] |= (v & 0x000f);
    tags1[2] |= ((v >> 4) & 0x000f);
    tags1[1] |= ((v >> 8) & 0x000f);
    tags1[3] |= ((v >> 12) & 0x000f);

    tags2[0] = (bucketbits2 >> 8) & kDirBitsMask;
    tags2[1] = (bucketbits2 >> 17) & kDirBitsMask;
    tags2[2] = (bucketbits2 >> 26) & kDirBitsMask;
    tags2[3] = (bucketbits2 >> 35) & kDirBitsMask;
    v = perm_.dec_table[(bucketbits2)&0x0fff];
    tags2[0] |= (v & 0x000f);
    tags2[2] |= ((v >> 4) & 0x000f);
    tags2[1] |= ((v >> 8) & 0x000f);
    tags2[3] |= ((v >> 12) & 0x000f);

    return (tags1[0] == tag) || (tags1[1] == tag) || (tags1[2] == tag) ||
           (tags1[3] == tag) || (tags2[0] == tag) || (tags2[1] == tag) ||
           (tags2[2] == tag) || (tags2[3] == tag);
  }

  bool FindTagInBucket(const size_t i, const uint32_t tag) const {
    DPRINTF(DEBUG_TABLE, "PackedTable::FindTagInBucket %zu\n", i);
    uint32_t tags[4];
    ReadBucket(i, tags);
    if (debug_level & DEBUG_TABLE) {
      PrintTags(tags);
    }

    bool ret = ((tags[0] == tag) || (tags[1] == tag) || (tags[2] == tag) ||
                (tags[3] == tag));
    DPRINTF(DEBUG_TABLE, "PackedTable::FindTagInBucket %d \n", ret);
    return ret;
  }

  bool DeleteTagFromBucket(const size_t i, const uint32_t tag) {
    uint32_t tags[4];
    ReadBucket(i, tags);
    if (debug_level & DEBUG_TABLE) {
      PrintTags(tags);
    }
    for (size_t j = 0; j < 4; j++) {
      if (tags[j] == tag) {
        tags[j] = 0;
        WriteBucket(i, tags);
        return true;
      }
    }
    return false;
  }  // DeleteTagFromBucket

  bool InsertTagToBucket(const size_t i, const uint32_t tag, const bool kickout,
                         uint32_t &oldtag) {
    DPRINTF(DEBUG_TABLE, "PackedTable::InsertTagToBucket %zu \n", i);

    uint32_t tags[4];
    DPRINTF(DEBUG_TABLE,
            "PackedTable::InsertTagToBucket read bucket to tags\n");
    ReadBucket(i, tags);
    if (debug_level & DEBUG_TABLE) {
      PrintTags(tags);
      PrintBucket(i);
    }
    for (size_t j = 0; j < 4; j++) {
      if (tags[j] == 0) {
        DPRINTF(DEBUG_TABLE,
                "PackedTable::InsertTagToBucket slot %zu is empty\n", j);

        tags[j] = tag;
        WriteBucket(i, tags);
        if (debug_level & DEBUG_TABLE) {
          PrintBucket(i);
          ReadBucket(i, tags);
        }
        DPRINTF(DEBUG_TABLE, "PackedTable::InsertTagToBucket Ok\n");
        return true;
      }
    }
    if (kickout) {
      size_t r = rand() & 3;
      DPRINTF(
          DEBUG_TABLE,
          "PackedTable::InsertTagToBucket, let's kick out a random slot %zu \n",
          r);
      // PrintBucket(i);

      oldtag = tags[r];
      tags[r] = tag;
      WriteBucket(i, tags);
      if (debug_level & DEBUG_TABLE) {
        PrintTags(tags);
      }
    }
    DPRINTF(DEBUG_TABLE, "PackedTable::InsertTagToBucket, insert failed \n");
    return false;
  }

  // inline size_t NumTagsInBucket(const size_t i) {
  //     size_t num = 0;
  //     for (size_t j = 0; j < tags_per_bucket; j++ ){
  //         if (ReadTag(i, j) != 0) {
  //             num ++;
  //         }
  //     }
  //     return num;
  // } // NumTagsInBucket

};  // PackedTable
}  // namespace cuckoofilter

#endif  // CUCKOO_FILTER_PACKED_TABLE_H_
