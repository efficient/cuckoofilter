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
// $ for num in 4321 654321 87654321; do echo $num:; /usr/bin/time -f 'time: %e seconds' ./bulk-insert-and-query.exe $num; echo; done
// 4321:
//                   Million    Find    Find    Find    Find    Find                       optimal  wasted
//                  adds/sec      0%     25%     50%     75%    100%       ε  bits/item  bits/item   space
//      Cuckoo12       69.01  199.22  200.63  200.66  200.73  180.44  0.097%      22.75      10.01  127.3%
//    SemiSort13       41.34   91.57   80.33   71.68   66.83   74.55  0.048%      22.76      11.03  106.4%
//       Cuckoo8       74.37  216.55  217.10  216.23  216.16  216.34  1.653%      15.17       5.92  156.2%
//     SemiSort9       43.14   92.32   91.80   90.15   89.42   87.94  1.056%      15.18       6.57  131.2%
//      Cuckoo16       74.50  210.66  210.71  211.21  211.09  211.44  0.007%      30.33      13.84  119.1%
//    SemiSort17       41.95   94.66   93.42   62.16   90.93   89.73  0.004%      30.35      14.72  106.1%
// SimdBlock12.5      284.95  289.72  288.60  289.99  288.71  278.78  0.176%      15.17       9.15   65.7%
// time: 7.12 seconds
//
// 654321:
//                   Million    Find    Find    Find    Find    Find                       optimal  wasted
//                  adds/sec      0%     25%     50%     75%    100%       ε  bits/item  bits/item   space
//      Cuckoo12       43.37  114.10  124.96  123.88  126.15  125.90  0.119%      19.23       9.72   97.9%
//    SemiSort13       26.64   44.83   44.14   41.62   42.50   50.10  0.060%      19.23      10.69   79.9%
//       Cuckoo8       57.38  141.78  139.67  141.85  140.96  140.15  1.955%      12.82       5.68  125.8%
//     SemiSort9       28.05   50.76   50.28   49.71   49.32   48.77  1.171%      12.82       6.42   99.8%
//      Cuckoo16       56.35  130.17  130.52  129.59  128.87  130.59  0.007%      25.64      13.74   86.6%
//    SemiSort17       26.73   47.04   46.57   46.16   42.92   45.54  0.003%      25.64      14.80   73.2%
// SimdBlock12.5      211.12  206.97  205.01  205.71  204.74  206.11  0.396%      12.82       7.98   60.7%
// time: 8.11 seconds
//
// 87654321:
//                   Million    Find    Find    Find    Find    Find                       optimal  wasted
//                  adds/sec      0%     25%     50%     75%    100%       ε  bits/item  bits/item   space
//      Cuckoo12       17.30   24.88   24.69   24.68   24.74   24.68  0.129%      18.37       9.60   91.5%
//    SemiSort13        7.58    9.92    9.86   10.05   11.61   16.14  0.066%      18.37      10.57   73.8%
//       Cuckoo8       21.45   27.13   26.85   27.18   27.25   27.04  2.028%      12.25       5.62  117.8%
//     SemiSort9        7.83   10.30   10.28   10.24   10.30   10.27  1.214%      12.25       6.36   92.5%
//      Cuckoo16       21.00   26.76   26.57   26.76   26.77   26.45  0.007%      24.50      13.87   76.7%
//    SemiSort17        7.65    9.87    9.92    9.86    9.92    9.94  0.004%      24.50      14.57   68.1%
// SimdBlock12.5       36.36   47.90   47.83   47.92   48.06   46.06  0.019%      24.50      12.33   98.7%
// time: 81.19 seconds

#include <climits>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <vector>

#include "cuckoofilter.h"
#include "random.h"
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

template<typename Table>
struct FilterAPI {};

template <typename ItemType, size_t bits_per_item, template <size_t> class TableType>
struct FilterAPI<CuckooFilter<ItemType, bits_per_item, TableType>> {
  using Table = CuckooFilter<ItemType, bits_per_item, TableType>;
  static Table ConstructFromAddCount(size_t add_count) { return Table(add_count); }
  static void Add(uint64_t key, Table * table) {
    if (0 != table->Add(key)) {
      throw logic_error("The filter is too small to hold all of the elements");
    }
  }
  static bool Contain(uint64_t key, const Table * table) {
    return (0 == table->Contain(key));
  }
};

template <>
struct FilterAPI<SimdBlockFilter> {
  using Table = SimdBlockFilter;
  static Table ConstructFromAddCount(size_t add_count) {
    Table ans(ceil(log2(add_count * 8.0 / CHAR_BIT)));
    return ans;
  }
  static void Add(uint64_t key, Table* table) {
    table->Add(key);
  }
  static bool Contain(uint64_t key, const Table * table) {
    return table->Find(key);
  }
};

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

  cf = FilterBenchmark<SimdBlockFilter>(add_count, to_add, to_lookup);

  cout << setw(NAME_WIDTH) << "SimdBlock8" << cf << endl;

}
