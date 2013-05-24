/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <iostream>
#include <vector>
#include <cmath>

#include <cassert>

//#include "singletable.h"
//#include "cptable.h"
//#include "packedtable.h"
#include "cp_packedtable.h"
#include "cuckoohashfilter.h"

using namespace std;
using namespace hashfilter;


int main(int argc, char** argv) {
//    const size_t bit_per_tag = 16;
    const size_t tags_per_bucket = 4;
    const size_t num_rows = (1 << 16);
    const int max_record =  4 * num_rows * tags_per_bucket;

    //CachePartitionedTable *T = new CachePartitionedTable(4, 12, tags_per_bucket, num_rows);
    //PackedTable *T = new PackedTable(5, num_rows);
    //CPPackedTable *T = new CPPackedTable(5, 8, num_rows);
    
    //CuckooHashFilter<SingleTable<16,4,1024,false> >  filter;
    //CuckooHashFilter<CachePartitionedTable<8,8,4,1024> >  filter;
    CuckooHashFilter<PackedTable<13,1024,false> >  filter;
    //CuckooHashFilter<CPPackedTable<, 8, 1024> >  filter;

    // preparing for the hashed keys
    vector<Value> keys;
    for ( int i = 0 ; i < max_record ; i++) {
        int num = i;
        string hashed_key= hashfilter::HashUtil::SHA1Hash((const char *)&num, sizeof(num));
        auto v = hashfilter::Value(hashed_key);
        keys.push_back(v);
    }

    cout << "sainty check\n";
    // inserting
    int i;
    cout << "start inserting key  0 ... " << max_record << "\n";
    cout << filter.Info() << endl;
    for ( i = 0 ; i < max_record ; i++) {
        auto v = keys[i];
        //cout << "\ninsert key" << i+1 <<": " << v->hexstr() << endl;
        if (filter.Add(v) == NotEnoughSpace) {
            cout << "insertion failed at key " << i  <<" :" << v.hexstr() << endl;;
            cout << "abort inserting" << endl;
            break;
        }
        else if (filter.Contains(v) != Ok) {
            cout << i << " kidding me!\n" ;
            return 0;
        }
    }
    cout << i << "keys inserted\n";
    cout << filter.Info() << endl;;
    int f = i;

    cout << endl;
    // checking
    int fp = 0;
    bool show = true;
    cout << "start checking existance for key 0 ... " << max_record << "\n";
    for ( int i = 0 ; i < max_record ; i++) {
        auto v = keys[i];
        //cout << "lookup key= " << v->hexstr() << endl;
        if (filter.Contains(v) == Ok ) {
            if  (i > f)
                fp ++;
        }
        else if (show) {
            cout << "first failed lookup is at key " << i  <<" :" << v.hexstr() << endl;
            show = false;
        }

    }
    double fpr = 1.0 * fp / (max_record - f);
    cout << "f.p. keys =" << fp << " out of " << (max_record-f) << " keys\n";
    cout << "f.p.r.="<< fpr <<", or " << -log2(fpr) << " bits/key" << endl;
    // clean up
    keys.clear();

    return 0;
 }
