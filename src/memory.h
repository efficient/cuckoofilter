#ifndef CUCKOO_FILTER_MEMORY_H_
#define CUCKOO_FILTER_MEMORY_H_

// This file provides two functions dealing with memory
// allocation. They abstract out complexities like allocating aligned
// memory and using 2MB "huge pages" rather than the usual 4KB pages,
// hopefully thereby reducing TLB misses.
//
// In benchmarking on 126M elements, this induced a <1.5% space
// increase, a 9% decrease in the wall-clock time to run
// bulk-insert-and-query.exe, as well as a 56% reduction in page
// faults and a 99% reduction in dTLB misses.

#include <cstddef>

#if defined(__GLIBC__) && defined(__GLIBC_MINOR__) &&                      \
    (((defined(_BSD_SOURCE) || defined(_SVID_SOURCE)) && __GLIBC__ == 2 && \
      __GLIBC_MINOR__ <= 19) ||                                            \
     (defined(_DEFAULT_SOURCE) &&                                          \
      ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 19))))

#define MMAP

#endif  // MMAP

static constexpr bool kMmap =
#if defined(MMAP)
    true;
#else
    false;
#endif  // MMAP

namespace cuckoofilter {

void *Allocate(std::size_t bytes, std::size_t *actual_bytes) noexcept(!kMmap);

void Deallocate(void *p, std::size_t bytes) noexcept(!kMmap);

}  // namespace cuckoofilter

#endif // CUCKOO_FILTER_MEMORY_H_
