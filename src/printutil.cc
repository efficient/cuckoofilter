#include "printutil.h"

#include <stdio.h>

#include <iostream>

namespace cuckoofilter {

std::string PrintUtil::bytes_to_hex(const char *data, size_t len) {
  std::string hexstr = "";
  static const char hexes[] = "0123456789ABCDEF ";

  for (size_t i = 0; i < len; i++) {
    unsigned char c = data[i];
    hexstr.push_back(hexes[c >> 4]);
    hexstr.push_back(hexes[c & 0xf]);
    hexstr.push_back(hexes[16]);
  }
  return hexstr;
};

std::string PrintUtil::bytes_to_hex(const std::string &s) {
  return bytes_to_hex((const char *)s.data(), s.size());
};

}  // namespace cuckoofilter
