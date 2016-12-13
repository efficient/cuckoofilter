// This benchmark reports on the bulk insert and bulk query rates. It is invoked as:
//
//     ./bulk-insert-and-query.exe 158000
//
// That invokation will test each probabilistic membership container type with 158000
// randomly generated items. It tests bulk Add() from empty to full and Contain() on
// filters with varying rates of expected success. For instance, at 75%, three out of
// every four values passed to Contain() were earlier Add()ed.
//
// Example output:
//
// $ for NUM in $(seq 15 17); do echo $NUM:; ./bulk-insert-and-query.exe ${NUM}000000; echo; done
// 15:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item     optimum for fpp      wasted space %
//   Cuckoo12               11.98     24.87     22.59     32.70     32.64     32.49              0.165%               13.42                9.24              45.21%
// SemiSort13                7.10     13.07     12.69     12.77     12.42     12.66              0.086%               13.42               10.18              31.82%
//    Cuckoo8               15.63     35.26     34.29     35.33     35.38     35.33              2.774%                8.95                5.17              73.02%
//  SemiSort9                7.97     14.33     15.01     14.93     14.64     14.74              1.424%                8.95                6.13              45.87%
//   Cuckoo16               13.20     33.08     33.05     33.08     32.98     33.02              0.012%               17.90               13.07              36.88%
// SemiSort17                6.90     11.84     12.50     12.48     11.51     12.30              0.005%               17.90               14.35              24.74%
//
// 16:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item     optimum for fpp      wasted space %
//   Cuckoo12                7.45     32.58     32.38     32.83     32.81     32.86              0.186%               12.58                9.07              38.68%
// SemiSort13                5.00     13.18     11.69     12.94     12.87     12.82              0.092%               12.58               10.08              24.81%
//    Cuckoo8                9.36     25.26     35.61     35.43     35.59     34.57              2.951%                8.39                5.08              65.05%
//  SemiSort9                5.68     14.89     15.16     15.18     14.66     14.64              1.508%                8.39                6.05              38.63%
//   Cuckoo16                7.54     33.11     33.10     33.13     33.12     32.58              0.010%               16.78               13.32              25.98%
// SemiSort17                4.77     12.52     12.60     12.55     12.46     11.55              0.005%               16.78               14.44              16.19%
//
// 17:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item     optimum for fpp      wasted space %
//   Cuckoo12               20.91     29.64     27.40     29.78     29.78     29.82              0.098%               23.69               10.00             136.91%
// SemiSort13                8.87     11.27     11.15     11.07     11.07     11.18              0.049%               23.69               10.99             115.42%
//    Cuckoo8               28.56     31.87     31.80     31.91     31.08     31.81              1.569%               15.79                5.99             163.43%
//  SemiSort9                9.45     11.48     12.30     12.26     11.80     10.72              1.043%               15.79                6.58             139.85%
//   Cuckoo16               26.84     30.74     30.64     30.42     29.16     30.68              0.007%               31.58               13.89             127.41%
// SemiSort17                8.57     11.12     11.05     11.01     11.05     10.57              0.004%               31.58               14.68             115.07%

#include <climits>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <vector>

#include "cuckoofilter.h"
#include "random.h"
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
  os << setw(20) << right << "adds per sec. (M)";
  for (int i = 0; i < find_percent_count; ++i) {
    os << setw(9)
       << static_cast<int>(100 * i / static_cast<double>(find_percent_count - 1)) << '%';
  }
  os << setw(20) << "false pos. prob." << setw(20) << "bits per item" << setw(20)
     << "optimum for fpp" << setw(20) << "wasted space %";
  return os.str();
}

// Overloading the usual operator<< as used in "std::cout << foo", but for Statistics
template <class CharT, class Traits>
basic_ostream<CharT, Traits>& operator<<(
    basic_ostream<CharT, Traits>& os, const Statistics& stats) {
  constexpr double NANOS_PER_MILLION = 1000;
  os << fixed << setprecision(2) << setw(20) << right
     << stats.adds_per_nano * NANOS_PER_MILLION;
  for (const auto& fps : stats.finds_per_nano) {
    os << setw(10) << fps.second * NANOS_PER_MILLION;
  }
  const auto minbits = log2(1 / stats.false_positive_probabilty);
  os << setw(19) << setprecision(3) << stats.false_positive_probabilty * 100 << '%'
     << setw(20) << setprecision(2) << stats.bits_per_item << setw(20) << minbits
     << setw(19) << 100 * (stats.bits_per_item / minbits - 1) << '%';

  return os;
}

template <typename Table>
Statistics CuckooBenchmark(
    size_t add_count, const vector<uint64_t>& to_add, const vector<uint64_t>& to_lookup) {
  if (add_count > to_add.size()) {
    throw out_of_range("to_add must contain at least add_count values");
  }

  if (SAMPLE_SIZE > to_lookup.size()) {
    throw out_of_range("to_lookup must contain at least SAMPLE_SIZE values");
  }

  Table cuckoo(add_count);
  Statistics result;

  // Add values until failure or until we run out of values to add:
  auto start_time = NowNanos();
  for (size_t added = 0; added < add_count; ++added) {
    if (0 != cuckoo.Add(to_add[added])) {
      throw logic_error("The filter is too small to hold all of the elements");
    }
  }
  result.adds_per_nano = add_count/static_cast<double>(NowNanos() - start_time);
  result.bits_per_item = static_cast<double>(CHAR_BIT * cuckoo.SizeInBytes()) / add_count;

  size_t found_count = 0;
  for (const double found_probability : {0.0, 0.25, 0.50, 0.75, 1.00}) {
    const auto to_lookup_mixed = MixIn(&to_lookup[0], &to_lookup[SAMPLE_SIZE], &to_add[0],
        &to_add[add_count], found_probability);
    const auto start_time = NowNanos();
    for (const auto v : to_lookup_mixed) found_count += (0 == cuckoo.Contain(v));
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

  cout << StatisticsTableHeader(10, 5) << endl;

  auto cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 12 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "Cuckoo12" << cf << endl;

  cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 13 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "SemiSort13" << cf << endl;

  cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 8 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "Cuckoo8" << cf << endl;

  cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 9 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "SemiSort9" << cf << endl;

  cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 16 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "Cuckoo16" << cf << endl;

  cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 17 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "SemiSort17" << cf << endl;
}
