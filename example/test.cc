/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#include "cuckoofilter.h"

using namespace cuckoofilter;

int main(int argc, char** argv) {
    const size_t bits_per_item = 12;
    const size_t total_items   = 1000000;

    // simple one:
    CuckooFilter<size_t, bits_per_item> filter(total_items);

    // insert items
    size_t num_inserted = 0;
    for (size_t i = 0; i < total_items; i++, num_inserted++) {
        if (filter.Add(i) != Ok) {
            break;
        }
    }

    // check existing items:
    // every item should be there
    for (size_t i = 0; i < num_inserted; i++) {
        assert(filter.Contain(i) == Ok);
    }

    // checking non-existing items
    // there are false positives
    size_t total_queries = 0;   
    size_t false_queries = 0;
    for (size_t i = total_items; i < 2 * total_items; i++) {
        if (filter.Contain(i) == Ok) {
            false_queries++;
        }
        total_queries++;
    }

    std::cout << "false positive rate is " << 100.0 * false_queries / total_queries << "%\n";

    return 0;
 }
