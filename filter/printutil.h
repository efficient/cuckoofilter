/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _PRINT_H_
#define _PRINT_H_

#include <string>
namespace hashfilter {
    class PrintUtil {

    public:
        static std::string bytes_to_hex(const char* data, size_t len);
        static std::string bytes_to_hex(const std::string& s);
    private:
        PrintUtil();
    }; // class PrintUtil

} // namespace hashfilter

#endif //_PRINT_H_
