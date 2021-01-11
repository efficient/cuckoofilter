#ifndef CUCKOO_FILTER_HASHUTIL_H_
#define CUCKOO_FILTER_HASHUTIL_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <iostream>
#include <fstream>

#include <string>

#include <openssl/evp.h>
#include <random>

namespace cuckoofilter {

class HashUtil {
 public:
  // Bob Jenkins Hash
  static uint32_t BobHash(const void *buf, size_t length, uint32_t seed = 0);
  static uint32_t BobHash(const std::string &s, uint32_t seed = 0);

  // Bob Jenkins Hash that returns two indices in one call
  // Useful for Cuckoo hashing, power of two choices, etc.
  // Use idx1 before idx2, when possible. idx1 and idx2 should be initialized to seeds.
  static void BobHash(const void *buf, size_t length, uint32_t *idx1,
                      uint32_t *idx2);
  static void BobHash(const std::string &s, uint32_t *idx1, uint32_t *idx2);

  // MurmurHash2
  static uint32_t MurmurHash(const void *buf, size_t length, uint32_t seed = 0);
  static uint32_t MurmurHash(const std::string &s, uint32_t seed = 0);

  // SuperFastHash
  static uint32_t SuperFastHash(const void *buf, size_t len);
  static uint32_t SuperFastHash(const std::string &s);

  // Null hash (shift and mask)
  static uint32_t NullHash(const void *buf, size_t length, uint32_t shiftbytes);

  // Wrappers for MD5 and SHA1 hashing using EVP
  static std::string MD5Hash(const char *inbuf, size_t in_length);
  static std::string SHA1Hash(const char *inbuf, size_t in_length);

 private:
  HashUtil();
};

// See Martin Dietzfelbinger, "Universal hashing and k-wise independent random
// variables via integer arithmetic without primes".
class TwoIndependentMultiplyShift {
  unsigned __int128 multiply_, add_;

 public:
  TwoIndependentMultiplyShift() {
    ::std::random_device random;
    for (auto v : {&multiply_, &add_}) {
      *v = random();
      for (int i = 1; i <= 4; ++i) {
        *v = *v << 32;
        *v |= random();
      }
    }
  }

  uint64_t operator()(uint64_t key) const {
    return (add_ + multiply_ * static_cast<decltype(multiply_)>(key)) >> 64;
  }

  void Serialize(std::ofstream& handler) {
      uint64_t bytes = sizeof(multiply_);
      handler.write(reinterpret_cast<char*>(&multiply_), bytes);
      std::cout << "Write multiply_ to file: total bytes: " << bytes << std::endl;
      bytes = sizeof(add_);
      handler.write(reinterpret_cast<char*>(&add_), bytes);
      std::cout << "Write add_ to file: total bytes: " << bytes << std::endl;
  }

  void Deserialize(std::ifstream& handler) {
      char* buffer = reinterpret_cast<char*>(&multiply_);
      uint64_t length = sizeof(multiply_);
      std::cout << "Read multiply_ from file: with size: " << length << std::endl;
      handler.read(buffer, length);
      buffer = reinterpret_cast<char*>(&add_);
      length = sizeof(add_);
      std::cout << "Read add_ from file: with size: " << length << std::endl;
      handler.read(buffer, length);
  }

};

// See Patrascu and Thorup's "The Power of Simple Tabulation Hashing"
class SimpleTabulation {
  uint64_t tables_[sizeof(uint64_t)][1 << CHAR_BIT];

 public:
  SimpleTabulation() {
    ::std::random_device random;
    for (unsigned i = 0; i < sizeof(uint64_t); ++i) {
      for (int j = 0; j < (1 << CHAR_BIT); ++j) {
        tables_[i][j] = random() | ((static_cast<uint64_t>(random())) << 32);
      }
    }
  }

  uint64_t operator()(uint64_t key) const {
    uint64_t result = 0;
    for (unsigned i = 0; i < sizeof(key); ++i) {
      result ^= tables_[i][reinterpret_cast<uint8_t *>(&key)[i]];
    }
    return result;
  }
  void Serialize(std::ofstream& handler) {
      int row = sizeof(uint64_t);
      int col = (1 << CHAR_BIT);
      uint64_t bytes = sizeof(uint64_t);
      uint64_t total_bytes = row * col * bytes;
      for (int i = 0; i < row; ++i) {
          for (int j = 0; j < col; ++j) {
              handler.write(reinterpret_cast<char*>(&tables_[i][j]), bytes);
          }
      }
      std::cout << "Write table_ to file: total bytes: " << total_bytes << std::endl;
  }

  void Deserialize(std::ifstream& handler) {
      int row = sizeof(uint64_t);
      int col = (1 << CHAR_BIT);
      uint64_t bytes = sizeof(uint64_t);
      uint64_t total_bytes = row * col * bytes;
      for (int i = 0; i < row; ++i) {
          for (int j = 0; j < col; ++j) {
              handler.read(reinterpret_cast<char*>(&tables_[i][j]), bytes);
          }
      }
      std::cout << "Read table_ to file: total bytes: " << total_bytes << std::endl;
  }
};
}

#endif  // CUCKOO_FILTER_HASHUTIL_H_
