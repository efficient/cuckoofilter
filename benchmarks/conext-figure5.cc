// This benchmark reproduces the CoNEXT 2014 results found in "Figure 5: Lookup
// performance when a filter achieves its capacity." It takes about two minutes to run on
// an Intel(R) Core(TM) i7-4790 CPU @ 3.60GHz.
//
// Results:
// fraction of queries on existing items/lookup throughput (million OPS)
//                      CF     ss-CF
//         0.00%     24.79      9.37
//        25.00%     24.65      9.57
//        50.00%     24.84      9.57
//        75.00%     24.86      9.62
//       100.00%     24.89      9.96

#include <climits>
#include <iomanip>
#include <vector>

#include "cuckoofilter.h"
#include "random.h"
#include "timing.h"

using namespace std;

using namespace cuckoofilter;

// The number of items sampled when determining the lookup performance
const size_t SAMPLE_SIZE = 1000 * 1000;

// The time (in seconds) to lookup SAMPLE_SIZE keys in which 0%, 25%, 50%, 75%, and 100%
// of the keys looked up are found.
template <typename Table>
array<double, 5> CuckooBenchmark(
    size_t add_count, const vector<uint64_t>& to_add, const vector<uint64_t>& to_lookup) {
  Table cuckoo(add_count);
  array<double, 5> result;

  // Add values until failure or until we run out of values to add:
  size_t added = 0;
  while (added < to_add.size() && 0 == cuckoo.Add(to_add[added])) ++added;

  // A value to track to prevent the compiler from optimizing out all lookups:
  size_t found_count = 0;
  for (const double found_percent : {0.0, 0.25, 0.50, 0.75, 1.00}) {
    const auto to_lookup_mixed = MixIn(&to_lookup[0], &to_lookup[SAMPLE_SIZE], &to_add[0],
        &to_add[added], found_percent);
    auto start_time = NowNanos();
    for (const auto v : to_lookup_mixed) found_count += (0 == cuckoo.Contain(v));
    auto lookup_time = NowNanos() - start_time;
    result[found_percent * 4] = lookup_time / (1000.0 * 1000.0 * 1000.0);
  }
  if (6 * SAMPLE_SIZE == found_count) exit(1);
  return result;
}

int main() {
  // Number of distinct values, used only for the constructor of CuckooFilter, which does
  // not allow the caller to specify the space usage directly. The actual number of
  // distinct items inserted depends on how many fit until an insert failure occurs.
  size_t add_count = 127.78 * 1000 * 1000;

  // Overestimate add_count so we don't run out of random data:
  const size_t max_add_count = 2 * add_count;
  const vector<uint64_t> to_add = GenerateRandom64(max_add_count);
  const vector<uint64_t> to_lookup = GenerateRandom64(SAMPLE_SIZE);

  // Calculate metrics:
  const auto cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 12 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);
  const auto sscf = CuckooBenchmark<
      CuckooFilter<uint64_t, 13 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << "fraction of queries on existing items/lookup throughput (million OPS) "
       << endl;
  cout << setw(10) << ""
       << " " << setw(10) << right << "CF" << setw(10) << right << "ss-CF" << endl;
  for (const double found_percent : {0.0, 0.25, 0.50, 0.75, 1.00}) {
    cout << fixed << setprecision(2) << setw(10) << right << 100 * found_percent << "%";
    cout << setw(10) << right << (SAMPLE_SIZE / cf[found_percent * 4]) / (1000 * 1000);
    cout << setw(10) << right << (SAMPLE_SIZE / sscf[found_percent * 4]) / (1000 * 1000);
    cout << endl;
  }
}
