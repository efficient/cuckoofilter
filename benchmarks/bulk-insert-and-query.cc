// This benchmark reports on the bulk insert and bulk query rates. It is invoked as:
//
//     ./bulk-insert-and-query.exe 158000
//
// That invocation will test each probabilistic membership container type with 158000
// randomly generated items. It tests bulk Add() from empty to full and Contain() on
// filters with varying rates of expected success. For instance, at 75%, three out of
// every four values passed to Contain() were earlier Add()ed.
//
// Example output:
//
// $ for num in 55 75 85; do echo $num:; /usr/bin/time -f 'time: %e seconds' ./bulk-insert-and-query.exe ${num}00000; echo; done
// 55:
//                   Million    Find    Find    Find    Find    Find                       optimal  wasted
//                  adds/sec      0%     25%     50%     75%    100%       ε  bits/item  bits/item   space
//      Cuckoo12       27.15   30.20   40.99   41.18   40.83   41.61  0.128%      18.30       9.61   90.5%
//    SemiSort13       11.21   18.29   18.15   18.26   18.46   17.55  0.065%      18.30      10.58   72.9%
//     Shingle12       21.34   40.58   40.80   40.82   40.66   40.91  0.062%      18.30      10.66   71.8%
//       Cuckoo8       42.06   45.61   54.74   53.58   55.83   54.35  2.071%      12.20       5.59  118.1%
//     SemiSort9       15.18   24.40   25.77   14.41   25.57   26.05  1.214%      12.20       6.36   91.7%
//      Cuckoo16       31.81   39.52   40.61   40.41   40.09   40.08  0.010%      24.40      13.30   83.5%
//    SemiSort17       11.24   16.73   16.55   16.71   16.77   16.34  0.005%      24.40      14.44   69.0%
//    SimdBlock8       81.48   84.58   86.63   86.63   83.58   87.26  0.485%      12.20       7.69   58.7%
// time: 14.06 seconds
//
// 75:
//                   Million    Find    Find    Find    Find    Find                       optimal  wasted
//                  adds/sec      0%     25%     50%     75%    100%       ε  bits/item  bits/item   space
//      Cuckoo12       18.27   41.87   41.80   40.89   39.83   41.95  0.170%      13.42       9.20   45.9%
//    SemiSort13        8.65   18.48   14.31   18.63   18.78   14.83  0.087%      13.42      10.17   31.9%
//     Shingle12       11.00   40.80   41.14   41.34   41.30   41.41  0.088%      13.42      10.16   32.1%
//       Cuckoo8       28.13   53.47   55.73   56.40   56.30   56.50  2.797%       8.95       5.16   73.4%
//     SemiSort9       12.43   25.76   26.30   25.91   16.99   26.46  1.438%       8.95       6.12   46.2%
//      Cuckoo16       17.71   40.93   41.09   41.19   41.31   40.84  0.012%      17.90      13.00   37.7%
//    SemiSort17        8.46   16.99   17.06   15.84   13.75   17.06  0.006%      17.90      14.10   26.9%
//    SimdBlock8       88.56   88.43   84.02   87.45   88.91   88.38  2.054%       8.95       5.61   59.6%
// time: 16.27 seconds
//
// 85:
//                   Million    Find    Find    Find    Find    Find                       optimal  wasted
//                  adds/sec      0%     25%     50%     75%    100%       ε  bits/item  bits/item   space
//      Cuckoo12       25.80   37.66   37.97   38.01   37.94   37.87  0.098%      23.69       9.99  137.1%
//    SemiSort13        9.60   14.38   14.51   14.34   12.69   14.56  0.048%      23.69      11.02  114.8%
//     Shingle12       21.77   37.25   36.65   37.44   37.55   35.79  0.052%      23.69      10.91  117.1%
//       Cuckoo8       36.73   40.92   40.99   41.51   40.96   41.39  1.574%      15.79       5.99  163.6%
//     SemiSort9       11.39   16.76   16.57   16.68   16.25   16.82  1.049%      15.79       6.57  140.2%
//      Cuckoo16       33.98   37.85   38.70   38.92   38.76   38.95  0.006%      31.58      13.98  125.9%
//    SemiSort17       10.30   13.39   14.30   14.21   14.34   14.40  0.004%      31.58      14.61  116.2%
//    SimdBlock8       66.62   72.34   72.50   71.38   72.43   72.09  0.141%      15.79       9.48   66.6%
// time: 16.50 seconds
//

#include <climits>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <vector>

#include "cuckoofilter.h"
#include "filter-api.h"
#include "random.h"
#include "shingle.h"
#include "simd-block.h"
#include "timing.h"

using namespace std;

using namespace cuckoofilter;

// The number of items sampled when determining the lookup performance
const size_t SAMPLE_SIZE = 1000 * 1000;

// The statistics gathered for each table type:
struct Statistics {
  double adds_per_nano;
  map<int, double> finds_per_nano; // The key is the percent of queries that were expected
                                   // to be positive
  double false_positive_probabilty;
  double bits_per_item;
};

// Output for the first row of the table of results. type_width is the maximum number of
// characters of the description of any table type, and find_percent_count is the number
// of different lookup statistics gathered for each table. This function assumes the
// lookup expected positive probabiilties are evenly distributed, with the first being 0%
// and the last 100%.
string StatisticsTableHeader(int type_width, int find_percent_count) {
  ostringstream os;

  os << string(type_width, ' ');
  os << setw(12) << right << "Million";
  for (int i = 0; i < find_percent_count; ++i) {
    os << setw(8) << "Find";
  }
  os << setw(8) << "" << setw(11) << "" << setw(11)
     << "optimal" << setw(8) << "wasted" << endl;

  os << string(type_width, ' ');
  os << setw(12) << right << "adds/sec";
  for (int i = 0; i < find_percent_count; ++i) {
    os << setw(7)
       << static_cast<int>(100 * i / static_cast<double>(find_percent_count - 1)) << '%';
  }
  os << setw(9) << "ε" << setw(11) << "bits/item" << setw(11)
     << "bits/item" << setw(8) << "space";
  return os.str();
}

// Overloading the usual operator<< as used in "std::cout << foo", but for Statistics
template <class CharT, class Traits>
basic_ostream<CharT, Traits>& operator<<(
    basic_ostream<CharT, Traits>& os, const Statistics& stats) {
  constexpr double NANOS_PER_MILLION = 1000;
  os << fixed << setprecision(2) << setw(12) << right
     << stats.adds_per_nano * NANOS_PER_MILLION;
  for (const auto& fps : stats.finds_per_nano) {
    os << setw(8) << fps.second * NANOS_PER_MILLION;
  }
  const auto minbits = log2(1 / stats.false_positive_probabilty);
  os << setw(7) << setprecision(3) << stats.false_positive_probabilty * 100 << '%'
     << setw(11) << setprecision(2) << stats.bits_per_item << setw(11) << minbits
     << setw(7) << setprecision(1) << 100 * (stats.bits_per_item / minbits - 1) << '%';

  return os;
}

template <typename Table>
Statistics FilterBenchmark(
    size_t add_count, const vector<uint64_t>& to_add, const vector<uint64_t>& to_lookup) {
  if (add_count > to_add.size()) {
    throw out_of_range("to_add must contain at least add_count values");
  }

  if (SAMPLE_SIZE > to_lookup.size()) {
    throw out_of_range("to_lookup must contain at least SAMPLE_SIZE values");
  }

  Table filter = FilterAPI<Table>::ConstructFromAddCount(add_count);
  Statistics result;

  // Add values until failure or until we run out of values to add:
  auto start_time = NowNanos();
  for (size_t added = 0; added < add_count; ++added) {
    FilterAPI<Table>::Add(to_add[added], &filter);
  }
  result.adds_per_nano = add_count / static_cast<double>(NowNanos() - start_time);
  result.bits_per_item = static_cast<double>(CHAR_BIT * filter.SizeInBytes()) / add_count;

  size_t found_count = 0;
  for (const double found_probability : {0.0, 0.25, 0.50, 0.75, 1.00}) {
    const auto to_lookup_mixed = MixIn(&to_lookup[0], &to_lookup[SAMPLE_SIZE], &to_add[0],
        &to_add[add_count], found_probability);
    const auto start_time = NowNanos();
    for (const auto v : to_lookup_mixed) {
      found_count += FilterAPI<Table>::Contain(v, &filter);
    }
    const auto lookup_time = NowNanos() - start_time;
    result.finds_per_nano[100 * found_probability] =
        SAMPLE_SIZE / static_cast<double>(lookup_time);
    if (0.0 == found_probability) {
      result.false_positive_probabilty =
          found_count / static_cast<double>(to_lookup_mixed.size());
    }
  }
  return result;
}

int main(int argc, char * argv[]) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " $NUMBER" << endl;
    return 1;
  }
  stringstream input_string(argv[1]);
  size_t add_count;
  input_string >> add_count;
  if (input_string.fail()) {
    cerr << "Invalid number: " << argv[1];
    return 2;
  }

  const vector<uint64_t> to_add = GenerateRandom64(add_count);
  const vector<uint64_t> to_lookup = GenerateRandom64(SAMPLE_SIZE);

  constexpr int NAME_WIDTH = 13;

  cout << StatisticsTableHeader(NAME_WIDTH, 5) << endl;

  auto cf = FilterBenchmark<
      CuckooFilter<uint64_t, 12 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "Cuckoo12" << cf << endl;

  cf = FilterBenchmark<
      CuckooFilter<uint64_t, 13 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "SemiSort13" << cf << endl;

  cf = FilterBenchmark<Shingle<>>(add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "Shingle12" << cf << endl;

  cf = FilterBenchmark<
      CuckooFilter<uint64_t, 8 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "Cuckoo8" << cf << endl;

  cf = FilterBenchmark<
      CuckooFilter<uint64_t, 9 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "SemiSort9" << cf << endl;

  cf = FilterBenchmark<
      CuckooFilter<uint64_t, 16 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "Cuckoo16" << cf << endl;

  cf = FilterBenchmark<
      CuckooFilter<uint64_t, 17 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "SemiSort17" << cf << endl;

  cf = FilterBenchmark<SimdBlockFilter<>>(add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "SimdBlock8" << cf << endl;

}
