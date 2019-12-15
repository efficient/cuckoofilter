#include "memory.h"

#include <cstdlib>
#include <cstring>

#if defined(MMAP)

#include <sys/mman.h>  // mmap, munmap
#include <cerrno>      // errno
#include <new>         // std::bad_alloc
#include <stdexcept>   // std::runtime_error

static constexpr uint64_t HUGE_PAGE_SIZE = ((uint64_t)1) << 21;

// OVERAGE_LIMIT is how much wiggle room there is on allocating more
// memory than specifically requested.
static constexpr double OVERAGE_LIMIT = 0.05;

#if defined(__linux__) && __linux__
#define MMAP_ZERO_FILLED true
#else
#define MMAP_ZERO_FILLED false
#endif  // __linux__

#endif  // MMAP

namespace cuckoofilter {

void *Allocate(std::size_t bytes, std::size_t *actual_bytes) noexcept(!kMmap) {
#if defined(MMAP)
  const double overage =
      static_cast<double>(HUGE_PAGE_SIZE - bytes % HUGE_PAGE_SIZE) /
      static_cast<double>(bytes);
  if (overage < OVERAGE_LIMIT) {
    bytes = ((bytes + HUGE_PAGE_SIZE - 1) / HUGE_PAGE_SIZE) * HUGE_PAGE_SIZE;
    *actual_bytes = bytes;
    errno = 0;
    void *result = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_HUGETLB | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == result) {
      throw std::runtime_error(std::strerror(errno));
    }
    if (!MMAP_ZERO_FILLED) std::memset(result, 0, bytes);
    return result;
  }
#endif  // MMAP
  *actual_bytes = bytes;
  void * result;
  const int malloc_failed = posix_memalign(&result, 64, bytes);
  if (malloc_failed) throw std::runtime_error(std::strerror(malloc_failed));
  std::memset(result, 0, bytes);
  return result;
}

void Deallocate(void *p, std::size_t bytes) noexcept(!kMmap) {
#if defined(MMAP)
  const double overage =
      static_cast<double>(HUGE_PAGE_SIZE - bytes % HUGE_PAGE_SIZE) /
      static_cast<double>(bytes);
  if (overage < OVERAGE_LIMIT) {
    bytes = ((bytes + HUGE_PAGE_SIZE - 1) / HUGE_PAGE_SIZE) * HUGE_PAGE_SIZE;
    errno = 0;
    const int fail = munmap(p, bytes);
    if (fail != 0) throw std::runtime_error(std::strerror(errno));
    return;
  }
#endif  // MMAP
  std::free(p);
}

}  // namespace cuckoofilter
