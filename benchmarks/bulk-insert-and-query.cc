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
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//   Cuckoo12               11.96     31.66     32.42     32.43     32.81     32.18              0.177%               13.42
// SemiSort13                7.10     13.04     12.88     12.86     12.80     12.80              0.090%               13.42
//    Cuckoo8               15.60     33.69     30.70     33.94     33.56     35.80              2.780%                8.95
//  SemiSort9                7.90     14.24     14.78     14.96     14.95     14.58              1.426%                8.95
//   Cuckoo16               13.16     33.02     33.19     32.56     32.93     31.84              0.012%               17.90
// SemiSort17                6.90     12.75     12.38     12.41     12.41     12.33              0.007%               17.90
//
// 16:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//   Cuckoo12                7.22     32.00     32.27     32.04     32.56     32.21              0.188%               12.58
// SemiSort13                4.85     12.66     12.81     12.53     12.68     12.58              0.093%               12.58
//    Cuckoo8                9.14     30.92     35.00     33.91     35.13     34.08              2.938%                8.39
//  SemiSort9                5.44     15.13     12.66     14.49     14.50     14.25              1.496%                8.39
//   Cuckoo16                7.52     32.19     33.27     32.14     32.33     32.18              0.013%               16.78
// SemiSort17                4.67     12.10     12.04     12.27     12.00     12.19              0.006%               16.78
//
// 17:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//   Cuckoo12               20.75     29.18     29.44     29.84     29.49     29.81              0.102%               23.69
// SemiSort13                8.81     11.13     10.69     11.06     11.05     11.38              0.052%               23.69
//    Cuckoo8               28.90     32.11     32.04     31.80     31.80     31.99              1.584%               15.79
//  SemiSort9                9.45     12.24     12.16     12.09     12.06     11.26              1.050%               15.79
//   Cuckoo16               26.87     30.78     30.85     30.77     30.80     30.89              0.006%               31.58
// SemiSort17                8.68     10.97     10.96     10.96     10.93     10.94              0.002%               31.58

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
  os << setw(20) << "false pos. prob." << setw(20) << "bits per item";
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
  os << setw(19) << setprecision(3) << stats.false_positive_probabilty * 100 << '%'
     << setw(20) << setprecision(2) << stats.bits_per_item;
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
