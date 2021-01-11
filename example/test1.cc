#include "cuckoofilter.h"

#include <assert.h>
#include <math.h>

#include <iostream>
#include <fstream>
#include <vector>

using cuckoofilter::CuckooFilter;

template<typename T>
void fpr(T& t, size_t total_items) {
  // Check non-existing items, a few false positives expected
  size_t total_queries = 0;
  size_t false_queries = 0;
  for (size_t i = total_items; i < 2 * total_items; i++) {
   if (t.Contain(i) == cuckoofilter::Ok) {
     false_queries++;
   }
   total_queries++;
  }

  // Output the measured false positive rate
  std::cout << "false positive rate is "
      << 100.0 * false_queries / total_queries << "%\n";
}

int main(int argc, char **argv) {
  size_t total_items = 1000000;

  // Create a cuckoo filter where each item is of type size_t and
  // use 12 bits for each item:
  //    CuckooFilter<size_t, 12> filter(total_items);
  // To enable semi-sorting, define the storage of cuckoo filter to be
  // PackedTable, accepting keys of size_t type and making 13 bits
  // for each key:
  CuckooFilter<size_t, 13, cuckoofilter::PackedTable> filter(total_items);
  // CuckooFilter<size_t, 12> filter(total_items);
  using FilterType = decltype(filter);

  // Insert items to this cuckoo filter
  size_t num_inserted = 0;
  for (size_t i = 0; i < total_items; i++, num_inserted++) {
    if (filter.Add(i) != cuckoofilter::Ok) {
      break;
    }
  }

  std::cout << "actual num_inserted: " << num_inserted << std::endl;
  num_inserted = 1000000;
  std::cout << "num_inserted: " << num_inserted << std::endl;

  // Check if previously inserted items are in the filter, expected
  // true for all items
  for (size_t i = 0; i < num_inserted; i++) {
    if(filter.Contain(i) != cuckoofilter::Ok) {
      std::cout << "I: " << i << " not ok" << std::endl;
      break;
    }
  }

  std::ofstream os("filter.meta", std::ios_base::binary);
  filter.Serialize(os);
  os.close();
  fpr(filter, total_items);

  FilterType filter1(total_items);
  std::ifstream handler("filter.meta", std::ios_base::binary);
  filter1.Deserialize(handler);
  handler.close();
  fpr(filter1, total_items);

  return 0;
}
