Cuckoo Filter
============

Overview
--------
Cuckoo filter is a Bloom filter replacement for approximated set-membership queries. While Bloom filters are well-known space-efficient data structures to serve queries like "if item x is in a set?", they do not support deletion. Their variances to enable deletion (like counting Bloom filters) usually require much more space.

Cuckoo ﬁlters provide the ﬂexibility to add and remove items dynamically. A cuckoo filter is based on cuckoo hashing (and therefore named as cuckoo filter).  It is essentially a cuckoo hash table storing each key's fingerprint. Cuckoo hash tables can be highly compact, thus a cuckoo filter could use less space than conventional Bloom ﬁlters, for applications that require low false positive rates (< 3%).

For details about the algorithm and citations please use:

["Cuckoo Filter: Practically Better Than Bloom"](http://www.cs.cmu.edu/~binfan/papers/conext14_cuckoofilter.pdf) in proceedings of ACM CoNEXT 2014 by Bin Fan, Dave Andersen and Michael Kaminsky


API
--------
A cuckoo filter supports following operations:

*  `Add(item)`: insert an item to the filter
*  `Contain(item)`: return if item is already in the filter. Note that this method may return false positive results like Bloom filters
*  `Delete(item)`: delete the given item from the filter. Note that to use this method, it must be ensured that this item is in the filter (e.g., based on records on external storage); otherwise, a false item may be deleted.
*  `Size()`: return the total number of items currently in the filter
*  `SizeInBytes()`: return the filter size in bytes

Here is a simple example in C++ for the basic usage of cuckoo filter.
More examples can be found in `example/` directory.

```cpp
// Create a cuckoo filter where each item is of type size_t and
// use 12 bits for each item, with capacity of total_items
CuckooFilter<size_t, 12> filter(total_items);
// Insert item 12 to this cuckoo filter
filter.Add(12);
// Check if previously inserted items are in the filter
assert(filter.Contain(12) == cuckoofilter::Ok);
```

Repository structure
--------------------
*  `src/`: the C++ header and implementation of cuckoo filter
*  `example/test.cc`: an example of using cuckoo filter
*  `benchmarks/`: Some benchmarks of speed, space used, and false positive rate


Build
-------
This libray depends on openssl library. Note that on MacOS 10.12, the header
files of openssl are not available by default. It may require to install openssl
and pass the path to `lib` and `include` directories to gcc, for example:

```bash
$ brew install openssl
# Replace 1.0.2j with the actual version of the openssl installed
$ export LDFLAGS="-L/usr/local/Cellar/openssl/1.0.2j/lib"
$ export CFLAGS="-I/usr/local/Cellar/openssl/1.0.2j/include"
```

To build the example (`example/test.cc`):
```bash
$ make test
```

To build the benchmarks:
```bash
$ cd benchmarks
$ make
```

Install
-------
To install the cuckoofilter library:
```bash
$ make install
```
By default, the header files will be placed in `/usr/local/include/cuckoofilter`
and the static library at `/usr/local/lib/cuckoofilter.a`.


Contributing
------------
Contributions via GitHub pull requests are welcome. Please keep the code style guided by
[Google C++ style](https://google.github.io/styleguide/cppguide.html). One can use
[clang-format](http://clang.llvm.org/docs/ClangFormat.html) with our provided
[`.clang-format`](https://github.com/efficient/cuckoofilter/blob/master/.clang-format)
in this repository to enforce the style.



Authors
-------
- Bin Fan <binfan@cs.cmu.edu>
- David G. Andersen <dga@cs.cmu.edu>
- Michael Kaminsky <michael.e.kaminsky@intel.com>
