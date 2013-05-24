/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define QUICK_N_DIRTY_HASHING

#include "../filter/filter_common.h"
#include "../filter/singletable.h"
#include "../filter/packedtable.h"
#include "../filter/cuckoofilter.h"
#include "../filter/bloomfilter.h"
#include "../filter/blockedbloomfilter.h"

using namespace std;
using namespace hashfilter;

std::mt19937 engine;  // mersenne twister

Value* keys = NULL;
const size_t num_rows = (1 << 25);
const size_t num_keys = num_rows * 4;
const size_t million  = 1000000;

const size_t recordlen = 8; // each key

double timeval_diff(const struct timeval * const start, const struct timeval * const end)
{
    /* Calculate the second difference*/
    double r = end->tv_sec - start->tv_sec;

    /* Calculate the microsecond difference */
    if (end->tv_usec > start->tv_usec) {
        r += (end->tv_usec - start->tv_usec)/1000000.0;
    } else if (end->tv_usec < start->tv_usec) {
        r -= (start->tv_usec - end->tv_usec)/1000000.0;
    }

    return r;
}


void init_keys(size_t num_keys, bool fast_gen)
{
    srand ( time(NULL) );
    engine.seed((uint64_t) rand());

    keys = new Value [num_keys];
    cout << "[init] init keys (fast key gen)...";
    size_t n_ints = recordlen/4;
    uint32_t kbuf[n_ints];
    for (size_t i = 0; i < num_keys; i++) {
        for (size_t j = 0; j < n_ints; j++) {
            kbuf[j] = engine();
        }
        keys[i] = Value((char *)kbuf, sizeof(kbuf));
    }
    cout << "done\n";
    cout << "[init] complete initializing " << num_keys << " keys\n";
    cout << "[init] space cost: " << 8 * num_keys / 1024.0 / 1024.0 << "MB\n";
}

void cleanup_keys() {
    cout << "[cleanup] cleanup keys...";
    delete [] keys;
    cout << "done\n";

}


template<typename FilterType>
void time_inserting(FilterType* filter) {
    struct timeval tv_s, tv_e;
    double total_time;

    cout << "\n";
    cout << "benchmarking insert performance\n";
    cout.flush();

    gettimeofday(&tv_s, NULL);

    size_t i = 0;
    while (( i < num_keys) && (filter->Add(keys[i]) == Ok)) {
        i++;
    }

    gettimeofday(&tv_e, NULL);

    total_time = timeval_diff(&tv_s, &tv_e);

    cout << "[insert] complete inserting " << i << " keys in " << total_time << " secs\n";
    printf("[insert] %.2f M queries / second,  %.2f ns\n",
           ((double)(i)/(total_time *1000000)),
           ((double) total_time) * 1000000000 / i);

    cout << "[insert] tput = " << i / total_time / million << " MOPS\n";
    cout << filter->Info() << endl;

}

template<typename FilterType>
void time_looking_up(FilterType* filter, float pos_frac) {

    struct timeval tv_s, tv_e;

    cout << "\nbenchmarking lookup performance with " << pos_frac*100 << "\% positive queries\n";
    cout.flush();


    size_t max_queries = 10000000;
    auto queries = new Value [max_queries];
    cout << "init lookup workload (fast key gen)...";
    size_t n_ints = recordlen/4;
    uint32_t kbuf[n_ints];
    for (size_t i = 0; i < max_queries; i++) {
        float  r = (double) rand() / RAND_MAX;
        if (r  > pos_frac) {
            // a negative query
            for (size_t j = 0; j < n_ints; j++) {
                kbuf[j] = engine();
            }
            queries[i] = Value((char *)kbuf, sizeof(kbuf));
        } else {
            // this is a positive query
            size_t j = rand() % filter->Size();
            queries[i] = keys[j];
        }
    }
    cout << "done\n";

    size_t false_ops  = 0;
    size_t total_ops = 0;
    double total_time = 0;


    while (total_time < 10) 
    {

        gettimeofday(&tv_s, NULL);
        for (size_t j = 0; j < max_queries; j++) {
            int res = (filter->Contain(queries[j]) == Ok);
            false_ops += res;
        }

        gettimeofday(&tv_e, NULL);
        total_time += timeval_diff(&tv_s, &tv_e);
        total_ops += max_queries;
    }

    cout <<  "[lookup] complete querying " << total_ops << " keys in " << total_time <<"sec\n";
    printf("[lookup] %.2f M queries / second,  %.2f ns,  f.p.r. = %f, bits/key= %.2f bit\n",
           ((double)(total_ops)/(total_time *1000000)),
           ((double) total_time) * 1000000000 / total_ops,
           1.0 * false_ops / total_ops, 
           filter->SizeInBytes() * 8.0 / filter->Size() );

}

template<typename FilterType>
void bench(float frac) {
    auto filter = new FilterType;
    time_inserting< FilterType >(filter);
    time_looking_up< FilterType >(filter, frac);
    delete filter;
}

void usage()
{
    cerr <<   "./bench_filter [options]" <<endl;
    cerr <<   "\t-s  :   benchmark the performance of looking up a singletable" << endl;
    cerr <<   "\t-p  :   benchmark the performance of looking up a packedtable" << endl;
    cerr <<   "\t-b  :   benchmark the performance of looking up a bloom filter" << endl;
    cerr <<   "\t-k  :   benchmark the performance of looking up a blocked bloom filter" << endl;
    cerr <<   "\t-f #:   fraction of positive queries in benchmarking" << endl;
}

int main(int argc, char** argv) {
    bool singletable    = false;
    bool packedtable    = false;
    bool bloomfilter    = false;
    bool blockedbf      = false;
    float frac          = 1.0;

    if (argc <= 1) {
        usage();
        exit(0);
    }

    int ch;
    while ((ch = getopt(argc, argv, "hspbkf:")) != -1) {
        switch (ch) {
        case 'h': usage(); exit(0); break;
        case 's': singletable    = true; break;
        case 'p': packedtable    = true; break;
        case 'b': bloomfilter    = true; break;
        case 'k': blockedbf      = true; break;
        case 'f': frac           = atof(optarg); break;
        default:
            usage();
            exit(-1);
        }
    }
    argc -= optind;
    argv += optind;

    // 
    // preparing for the hashed keys
    //
    init_keys(num_keys, true);

    //
    // this SingleTable-only case should be treated as an in-cache table.
    // because there is nothing preventing it from using cache freely
    //
    if (singletable) {
        //bench< BlockedCuckooFilter< SingleTable<12, 4, num_rows, true> > >(frac);
        bench< CuckooFilter< SingleTable<12, 4, num_rows, true> > >(frac);
    }

    //
    // cuckoo filter with 1 bit per bucket saved
    //
    if (packedtable)
        bench< CuckooFilter< PackedTable<13, num_rows, false> > >(frac);

    //
    // Bloom filter
    //
	if (bloomfilter) {
		//bench< BloomFilter<4 * num_keys, 8 * num_keys, 3> >();
		bench< BloomFilter<13 * num_keys, 9> >(frac);
	}

    //
    // Blocked Bloom filter
    //
    if (blockedbf) {
        bench< BlockedBloomFilter<13 * num_keys, 9> >(frac);
    }

    cleanup_keys();

    return 0;
}
