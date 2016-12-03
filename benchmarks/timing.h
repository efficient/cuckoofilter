// Timers for use in benchmarking.

#pragma once

#include <cstdint>
#include <chrono>

::std::uint64_t NowNanos() {
  return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(
             ::std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
