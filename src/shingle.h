#pragma once

// Cuckoo filters in which the buckets can overlap. See Lehman, Eric, and Rina
// Panigrahy. "3.5-way cuckoo hashing for the price of 2-and-a-bit." European
// Symposium on Algorithms. Springer, Berlin, Heidelberg, 2009.

#include <array>
#include <cstdint>
#include <cstdlib>
#include <random>

#include "bitsutil.h"
#include "hashutil.h"

template <typename HashFamily = ::cuckoofilter::TwoIndependentMultiplyShift>
class Shingle {
  using uint16_t = ::std::uint16_t;
  using uint64_t = ::std::uint64_t;

  // The low-order `bits` bits of the result are 1; all others are 0.
  static constexpr uint64_t Mask(int bits) {
    return (static_cast<uint64_t>(1) << bits) - 1;
  }

  // The two halves of the table are stored interleaved, A[0] then B[0] then
  // A[1] then B[1], and so on. Each slot has 12 bits, and we store A[i] and
  // B[i] together in a `Cell` of three bytes (24 bits).  We use the eleven
  // high-order bits to store a fingerprint and the bottom bit to indicate if
  // the fingerprint is offset from the original bucket it hashed to.
  //
  // In this class and below, methods that can operate on Cells have a
  // template <bool ISA> parameter that is true if the value from the array A is
  // to be manipulated and false if the value from the array B is to be
  // manipulated.
  //
  // The fingerprint 0x0 is reserved to indicate an empty slot. Keys hashing to
  // 0x0 are considered to have a hash of 0x1.

  using Cell = ::std::array<char, 3>;

  static_assert(sizeof(Cell[3]) == 9, "Cells are not packed tightly");

  HashFamily hasher_;
  // A and B have the same length, which is a power of 2. imask_ is one less
  // than that length
  const uint64_t imask_;
  // fp_hash_ uses delta-universal hashing (of the multiply-shift type) to
  // derive an index in B from the index in A plus a hash of the fingerprint.
  const uint64_t fp_hash_;
  Cell *const data_;
  size_t filled_;  // Number of non-empty slots.

  // Get the fingerprint and offset from index i. The table is A if ISA is true.
  template <bool ISA>
  [[gnu::always_inline]] uint64_t Get(uint64_t i) const {
    const uint16_t result =
        *reinterpret_cast<const uint16_t *>(&data_[i][1 - ISA]);
    if (ISA) {
      return result & 0x0fff;
    } else {
      return result >> 4;
    }
  }

  // Set the fingerprint and offset at index i to the low-order 12 bits of
  // x. The table is A if ISA is true.
  template <bool ISA>
  [[gnu::always_inline]] void Set(uint64_t i, uint64_t x) {
    uint16_t &result = *reinterpret_cast<uint16_t *>(&data_[i][1 - ISA]);
    if (ISA) {
      result = x | (result & 0xf000);
    } else {
      result = (x << 4) | (result & 0x000f);
    }
  }

  uint64_t ReIndex(uint64_t idx, uint64_t fp) const {
    return (idx ^ ((fp_hash_ * fp) >> 11)) & imask_;
  }

  // Set (ISA ? A : B)[idx + offset] = fp and return the index and fingerpritn
  // that was previously there.
  template <bool ISA>
  [[gnu::always_inline]] void Swap(uint64_t idx, uint64_t offset, uint64_t fp,
                                   uint64_t *result_idx,
                                   uint64_t *result_fp) {
    idx += offset;
    fp = offset | (fp << 1);
    *result_idx = idx;
    *result_fp = Get<ISA>(idx);
    if (*result_fp & 1) --*result_idx;
    *result_fp >>= 1;
    Set<ISA>(idx, fp);
  }

  // Helper function for Add(), below. Places fp in one of its two slots (idx or
  // idx+1) in (ISA ? A : B), and recurses if necessary.
  template <bool ISA>
  void AddHelp(uint64_t idx, uint64_t fp) {
    for (uint64_t offset : {0, 1}) {
      const uint64_t q = idx + offset;
      const uint64_t fp_now = Get<ISA>(q);
      if (0 == fp_now) {
        uint64_t fp_later = offset | (fp << 1);
        Set<ISA>(q, fp_later);
        ++filled_;
        return;
      }
    }

    // Do a short local search to see if some items in the next bucket can be
    // pushed to later slots, ala robin-hood linear probing.
    if (0 == (Get<ISA>(idx + 1) & 0x1)) {
      if (0 == Get<ISA>(idx + 2)) {
        Set<ISA>(idx + 2, 0x1 | Get<ISA>(idx + 1));
        Set<ISA>(idx + 1, 0x1 | (fp << 1));
        ++filled_;
        return;
      } else if (0 == (Get<ISA>(idx + 2) & 0x1)) {
        if (0 == Get<ISA>(idx + 3)) {
          Set<ISA>(idx + 3, 0x1 | Get<ISA>(idx + 2));
          Set<ISA>(idx + 2, 0x1 | Get<ISA>(idx + 1));
          Set<ISA>(idx + 1, 0x1 | (fp << 1));
          ++filled_;
          return;
        }
      }
    }

    // Kick out a random key from the two slots:
    uint64_t offset = std::rand() % 2;
    // TODO: replace random search with BFS or iterative deepening
    Swap<ISA>(idx, offset, fp, &idx, &fp);
    // TODO: replace recursion with iteration
    return AddHelp<!ISA>(ReIndex(idx, fp), fp);
  }

  // Helper for Delete(), below. Returns true if the key was found.
  template <bool ISA = true>
  [[gnu::always_inline]] bool DeleteHelp(uint64_t idx, uint64_t fp) {
    for (uint64_t offset : {0, 1}) {
      uint64_t i = idx + offset, f = offset | (fp << 1);
      if (Get<ISA>(i) == f) {
        Set<ISA>(i, 0);
        return true;
      }
    }
    if (ISA) return DeleteHelp<false>(ReIndex(idx, fp), fp);
    return false;
  }

 public:
  explicit Shingle(int log2_slots)
      : hasher_(),
        // Each array has half of the slots
        imask_(Mask(log2_slots - 1)),
        fp_hash_([]() {
          ::std::random_device random;
          uint64_t result = random();
          return (result << 32) | random();
        }()),
        // Add two extra SlotPairs at the end so 64-bit operations don't read
        // past the end and SEGFAULT.
        data_(new Cell[imask_ + 3]()),
        filled_(0) {}

  ~Shingle() { delete[] data_; }

  uint64_t SizeInBytes() const { return sizeof(Cell) * (imask_ + 3); }

  bool Add(uint64_t key) {
    if ((static_cast<double>(filled_) / (2 * (imask_ + 1))) > (12.0 / 12.75)) {
      return false;
    }
    key = hasher_(key);
    uint64_t idx  = (key >> 11) & imask_, fp = key & Mask(11);
    fp += (0 == fp);  // Since 0 is the empty slot, re-target zero remainders.
    AddHelp<true>(idx, fp);
    return true;
  }

  [[gnu::always_inline]] bool Contain(uint64_t key) const {
    key = hasher_(key);
    uint64_t idx = (key >> 11) & imask_, fp = key & Mask(11);
    fp += (fp == 0);
    auto idx2 = ReIndex(idx, fp);
    constexpr uint64_t A_SLOTS_MASK = Mask(12) + (Mask(12) << 24),
                       B_SLOTS_MASK = A_SLOTS_MASK << 12;
    uint64_t slots =
        (~A_SLOTS_MASK) | *reinterpret_cast<const uint64_t *>(&data_[idx]);
    auto slots2 =
        (~B_SLOTS_MASK) | *reinterpret_cast<const uint64_t *>(&data_[idx2]);
    auto slots_all = slots & slots2;

    uint64_t fp_all = fp * 0x002002002002ull;
    fp_all |= 0x001001000000ull;

    return haszero12(fp_all ^ slots_all);
  }

  bool Delete(uint64_t key) {
    key = hasher_(key);
    const uint64_t idx = (key >> 11) & imask_;
    uint64_t fp = key & Mask(11);
    fp += (0 == fp);
    return DeleteHelp<>(idx, fp);
  }
};
