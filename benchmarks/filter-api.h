#pragma once

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include "cuckoofilter.h"
#include "shingle.h"
#include "simd-block.h"

template <typename Table>
struct FilterAPI {};

template <typename ItemType, std::size_t bits_per_item,
          template <std::size_t> class TableType>
struct FilterAPI<
    cuckoofilter::CuckooFilter<ItemType, bits_per_item, TableType>> {
  using Table = cuckoofilter::CuckooFilter<ItemType, bits_per_item, TableType>;
  static Table ConstructFromAddCount(std::size_t add_count) {
    return Table(add_count);
  }
  static void Add(std::uint64_t key, Table *table) {
    if (0 != table->Add(key)) {
      throw std::logic_error(
          "The cuckoo filter is too small to hold all of the elements");
    }
  }
  static bool Contain(std::uint64_t key, const Table *table) {
    return (0 == table->Contain(key));
  }
};

template <>
struct FilterAPI<SimdBlockFilter<>> {
  using Table = SimdBlockFilter<>;
  static Table ConstructFromAddCount(std::size_t add_count) {
    Table ans(std::ceil(std::log2(add_count * 8.0 / CHAR_BIT)));
    return ans;
  }
  static void Add(std::uint64_t key, Table *table) { table->Add(key); }
  static bool Contain(std::uint64_t key, const Table *table) {
    return table->Find(key);
  }
};

template <typename HashFamily>
struct FilterAPI<Shingle<HashFamily>> {
  using Table = Shingle<HashFamily>;
  static Table ConstructFromAddCount(size_t add_count) {
    return Table(ceil(log2(add_count * 12.75 / 12.0)));
  }
  static void Add(std::uint64_t key, Table *table) {
    if (!table->Add(key)) {
      throw std::logic_error(
          "The quotient filter is too small to hold all of the elements");
    }
  }
  static bool Contain(std::uint64_t key, const Table *table) {
    return table->Contain(key);
  }
};
