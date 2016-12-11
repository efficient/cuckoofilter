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
// $ for NUM in 1234 12345 123456 1234567; do echo $NUM:; ./bulk-insert-and-query.exe $NUM; echo; done
// 1234:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//     Cuckoo               58.91    189.60    189.68    189.92    189.38    189.27              0.114%               19.92
//   SemiSort               37.92     87.60     80.13     73.21     66.40     61.88              0.059%               19.96
//
// 12345:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//     Cuckoo               47.62    188.88    188.80    188.64    182.71    189.28              0.147%               15.93
//   SemiSort               26.29     85.85     77.18     70.02     52.29     59.10              0.071%               15.93
//
// 123456:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//     Cuckoo               31.24    170.39    165.96    170.83    170.79    170.78              0.182%               12.74
//   SemiSort               19.37     73.69     67.60     60.93     55.67     51.47              0.091%               12.74
//
// 1234567:
//              adds per sec. (M)        0%       25%       50%       75%      100%    false pos. prob.       bits per item
//     Cuckoo               42.52    106.25     97.39    107.50    107.43     99.92              0.115%               20.38
//   SemiSort               25.74     42.34     40.56     37.54     37.04     34.59              0.060%               20.38

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

  const auto cf = CuckooBenchmark<
      CuckooFilter<uint64_t, 12 /* bits per item */, SingleTable /* not semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "Cuckoo" << cf << endl;

  const auto sscf = CuckooBenchmark<
      CuckooFilter<uint64_t, 13 /* bits per item */, PackedTable /* semi-sorted*/>>(
      add_count, to_add, to_lookup);

  cout << setw(10) << "SemiSort" << sscf << endl;
}
